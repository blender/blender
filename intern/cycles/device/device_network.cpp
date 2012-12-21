/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "device.h"
#include "device_intern.h"
#include "device_network.h"

#include "util_foreach.h"

CCL_NAMESPACE_BEGIN

#ifdef WITH_NETWORK

class NetworkDevice : public Device
{
public:
	boost::asio::io_service io_service;
	tcp::socket socket;
	device_ptr mem_counter;
	DeviceTask the_task; /* todo: handle multiple tasks */

	NetworkDevice(Stats &stats, const char *address)
	: Device(stats), socket(io_service)
	{
		stringstream portstr;
		portstr << SERVER_PORT;

		tcp::resolver resolver(io_service);
		tcp::resolver::query query(address, portstr.str());
		tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
		tcp::resolver::iterator end;

		boost::system::error_code error = boost::asio::error::host_not_found;
		while(error && endpoint_iterator != end)
		{
			socket.close();
			socket.connect(*endpoint_iterator++, error);
		}

		if(error)
			throw boost::system::system_error(error);

		mem_counter = 0;
	}

	~NetworkDevice()
	{
		RPCSend snd(socket, "stop");
		snd.write();
	}

	void mem_alloc(device_memory& mem, MemoryType type)
	{
		mem.device_pointer = ++mem_counter;

		RPCSend snd(socket, "mem_alloc");

		snd.add(mem);
		snd.add(type);
		snd.write();
	}

	void mem_copy_to(device_memory& mem)
	{
		RPCSend snd(socket, "mem_copy_to");

		snd.add(mem);
		snd.write();
		snd.write_buffer((void*)mem.data_pointer, mem.memory_size());
	}

	void mem_copy_from(device_memory& mem, int y, int w, int h, int elem)
	{
		RPCSend snd(socket, "mem_copy_from");

		snd.add(mem);
		snd.add(y);
		snd.add(w);
		snd.add(h);
		snd.add(elem);
		snd.write();

		RPCReceive rcv(socket);
		rcv.read_buffer((void*)mem.data_pointer, mem.memory_size());
	}

	void mem_zero(device_memory& mem)
	{
		RPCSend snd(socket, "mem_zero");

		snd.add(mem);
		snd.write();
	}

	void mem_free(device_memory& mem)
	{
		if(mem.device_pointer) {
			RPCSend snd(socket, "mem_free");

			snd.add(mem);
			snd.write();

			mem.device_pointer = 0;
		}
	}

	void const_copy_to(const char *name, void *host, size_t size)
	{
		RPCSend snd(socket, "const_copy_to");

		string name_string(name);

		snd.add(name_string);
		snd.add(size);
		snd.write();
		snd.write_buffer(host, size);
	}

	void tex_alloc(const char *name, device_memory& mem, bool interpolation, bool periodic)
	{
		mem.device_pointer = ++mem_counter;

		RPCSend snd(socket, "tex_alloc");

		string name_string(name);

		snd.add(name_string);
		snd.add(mem);
		snd.add(interpolation);
		snd.add(periodic);
		snd.write();
		snd.write_buffer((void*)mem.data_pointer, mem.memory_size());
	}

	void tex_free(device_memory& mem)
	{
		if(mem.device_pointer) {
			RPCSend snd(socket, "tex_free");

			snd.add(mem);
			snd.write();

			mem.device_pointer = 0;
		}
	}

	void task_add(DeviceTask& task)
	{
		the_task = task;

		RPCSend snd(socket, "task_add");
		snd.add(task);
		snd.write();
	}

	void task_wait()
	{
		RPCSend snd(socket, "task_wait");
		snd.write();

		list<RenderTile> the_tiles;

		/* todo: run this threaded for connecting to multiple clients */
		for(;;) {
			RPCReceive rcv(socket);
			RenderTile tile;

			if(rcv.name == "acquire_tile") {
				/* todo: watch out for recursive calls! */
				if(the_task.acquire_tile(this, tile)) { /* write return as bool */
					the_tiles.push_back(tile);

					RPCSend snd(socket, "acquire_tile");
					snd.add(tile);
					snd.write();
				}
				else {
					RPCSend snd(socket, "acquire_tile_none");
					snd.write();
				}
			}
			else if(rcv.name == "release_tile") {
				rcv.read(tile);

				for(list<RenderTile>::iterator it = the_tiles.begin(); it != the_tiles.end(); it++) {
					if(tile.x == it->x && tile.y == it->y && tile.start_sample == it->start_sample) {
						tile.buffers = it->buffers;
						the_tiles.erase(it);
						break;
					}
				}

				assert(tile.buffers != NULL);

				the_task.release_tile(tile);

				RPCSend snd(socket, "release_tile");
				snd.write();
			}
			else if(rcv.name == "task_wait_done")
				break;
		}
	}

	void task_cancel()
	{
		RPCSend snd(socket, "task_cancel");
		snd.write();
	}

	bool support_advanced_shading()
	{
		return true; /* todo: get this info from device */
	}
};

Device *device_network_create(DeviceInfo& info, Stats &stats, const char *address)
{
	return new NetworkDevice(stats, address);
}

void device_network_info(vector<DeviceInfo>& devices)
{
	DeviceInfo info;

	info.type = DEVICE_NETWORK;
	info.description = "Network Device";
	info.id = "NETWORK";
	info.num = 0;

	devices.push_back(info);
}

class DeviceServer {
public:
	DeviceServer(Device *device_, tcp::socket& socket_)
	: device(device_), socket(socket_)
	{
	}

	void listen()
	{
		/* receive remote function calls */
		for(;;) {
			RPCReceive rcv(socket);

			if(rcv.name == "stop")
				break;

			process(rcv);
		}
	}

protected:
	void process(RPCReceive& rcv)
	{
		// fprintf(stderr, "receive process %s\n", rcv.name.c_str());

		if(rcv.name == "mem_alloc") {
			MemoryType type;
			network_device_memory mem;
			device_ptr remote_pointer;

			rcv.read(mem);
			rcv.read(type);

			/* todo: CPU needs mem.data_pointer */

			remote_pointer = mem.device_pointer;

			mem_data[remote_pointer] = vector<uint8_t>();
			mem_data[remote_pointer].resize(mem.memory_size());
			if(mem.memory_size())
				mem.data_pointer = (device_ptr)&(mem_data[remote_pointer][0]);
			else
				mem.data_pointer = 0;

			device->mem_alloc(mem, type);

			ptr_map[remote_pointer] = mem.device_pointer;
			ptr_imap[mem.device_pointer] = remote_pointer;
		}
		else if(rcv.name == "mem_copy_to") {
			network_device_memory mem;

			rcv.read(mem);

			device_ptr remote_pointer = mem.device_pointer;
			mem.data_pointer = (device_ptr)&(mem_data[remote_pointer][0]);

			rcv.read_buffer((uint8_t*)mem.data_pointer, mem.memory_size());

			mem.device_pointer = ptr_map[remote_pointer];

			device->mem_copy_to(mem);
		}
		else if(rcv.name == "mem_copy_from") {
			network_device_memory mem;
			int y, w, h, elem;

			rcv.read(mem);
			rcv.read(y);
			rcv.read(w);
			rcv.read(h);
			rcv.read(elem);

			device_ptr remote_pointer = mem.device_pointer;
			mem.device_pointer = ptr_map[remote_pointer];
			mem.data_pointer = (device_ptr)&(mem_data[remote_pointer][0]);

			device->mem_copy_from(mem, y, w, h, elem);

			RPCSend snd(socket);
			snd.write();
			snd.write_buffer((uint8_t*)mem.data_pointer, mem.memory_size());
		}
		else if(rcv.name == "mem_zero") {
			network_device_memory mem;
			
			rcv.read(mem);
			device_ptr remote_pointer = mem.device_pointer;
			mem.device_pointer = ptr_map[mem.device_pointer];
			mem.data_pointer = (device_ptr)&(mem_data[remote_pointer][0]);

			device->mem_zero(mem);
		}
		else if(rcv.name == "mem_free") {
			network_device_memory mem;
			device_ptr remote_pointer;

			rcv.read(mem);

			remote_pointer = mem.device_pointer;
			mem.device_pointer = ptr_map[mem.device_pointer];
			ptr_map.erase(remote_pointer);
			ptr_imap.erase(mem.device_pointer);
			mem_data.erase(remote_pointer);

			device->mem_free(mem);
		}
		else if(rcv.name == "const_copy_to") {
			string name_string;
			size_t size;

			rcv.read(name_string);
			rcv.read(size);

			vector<char> host_vector(size);
			rcv.read_buffer(&host_vector[0], size);

			device->const_copy_to(name_string.c_str(), &host_vector[0], size);
		}
		else if(rcv.name == "tex_alloc") {
			network_device_memory mem;
			string name;
			bool interpolation;
			bool periodic;
			device_ptr remote_pointer;

			rcv.read(name);
			rcv.read(mem);
			rcv.read(interpolation);
			rcv.read(periodic);

			remote_pointer = mem.device_pointer;

			mem_data[remote_pointer] = vector<uint8_t>();
			mem_data[remote_pointer].resize(mem.memory_size());
			if(mem.memory_size())
				mem.data_pointer = (device_ptr)&(mem_data[remote_pointer][0]);
			else
				mem.data_pointer = 0;

			rcv.read_buffer((uint8_t*)mem.data_pointer, mem.memory_size());

			device->tex_alloc(name.c_str(), mem, interpolation, periodic);

			ptr_map[remote_pointer] = mem.device_pointer;
			ptr_imap[mem.device_pointer] = remote_pointer;
		}
		else if(rcv.name == "tex_free") {
			network_device_memory mem;
			device_ptr remote_pointer;

			rcv.read(mem);

			remote_pointer = mem.device_pointer;
			mem.device_pointer = ptr_map[mem.device_pointer];
			ptr_map.erase(remote_pointer);
			ptr_map.erase(mem.device_pointer);
			mem_data.erase(remote_pointer);

			device->tex_free(mem);
		}
		else if(rcv.name == "task_add") {
			DeviceTask task;

			rcv.read(task);

			if(task.buffer) task.buffer = ptr_map[task.buffer];
			if(task.rgba) task.rgba = ptr_map[task.rgba];
			if(task.shader_input) task.shader_input = ptr_map[task.shader_input];
			if(task.shader_output) task.shader_output = ptr_map[task.shader_output];

			task.acquire_tile = function_bind(&DeviceServer::task_acquire_tile, this, _1, _2);
			task.release_tile = function_bind(&DeviceServer::task_release_tile, this, _1);
			task.update_progress_sample = function_bind(&DeviceServer::task_update_progress_sample, this);
			task.update_tile_sample = function_bind(&DeviceServer::task_update_tile_sample, this, _1);
			task.get_cancel = function_bind(&DeviceServer::task_get_cancel, this);

			device->task_add(task);
		}
		else if(rcv.name == "task_wait") {
			device->task_wait();

			RPCSend snd(socket, "task_wait_done");
			snd.write();
		}
		else if(rcv.name == "task_cancel") {
			device->task_cancel();
		}
	}

	bool task_acquire_tile(Device *device, RenderTile& tile)
	{
		thread_scoped_lock acquire_lock(acquire_mutex);

		bool result = false;

		RPCSend snd(socket, "acquire_tile");
		snd.write();

		while(1) {
			RPCReceive rcv(socket);

			if(rcv.name == "acquire_tile") {
				rcv.read(tile);

				if(tile.buffer) tile.buffer = ptr_map[tile.buffer];
				if(tile.rng_state) tile.rng_state = ptr_map[tile.rng_state];
				if(tile.rgba) tile.rgba = ptr_map[tile.rgba];

				result = true;
				break;
			}
			else if(rcv.name == "acquire_tile_none")
				break;
			else
				process(rcv);
		}

		return result;
	}

	void task_update_progress_sample()
	{
		; /* skip */
	}

	void task_update_tile_sample(RenderTile&)
	{
		; /* skip */
	}

	void task_release_tile(RenderTile& tile)
	{
		thread_scoped_lock acquire_lock(acquire_mutex);

		if(tile.buffer) tile.buffer = ptr_imap[tile.buffer];
		if(tile.rng_state) tile.rng_state = ptr_imap[tile.rng_state];
		if(tile.rgba) tile.rgba = ptr_imap[tile.rgba];

		RPCSend snd(socket, "release_tile");
		snd.add(tile);
		snd.write();

		while(1) {
			RPCReceive rcv(socket);

			if(rcv.name == "release_tile")
				break;
			else
				process(rcv);
		}
	}

	bool task_get_cancel()
	{
		return false;
	}

	/* properties */
	Device *device;
	tcp::socket& socket;

	/* mapping of remote to local pointer */
	map<device_ptr, device_ptr> ptr_map;
	map<device_ptr, device_ptr> ptr_imap;
	map<device_ptr, vector<uint8_t> > mem_data;

	thread_mutex acquire_mutex;

	/* todo: free memory and device (osl) on network error */
};

void Device::server_run()
{
	try {
		/* starts thread that responds to discovery requests */
		ServerDiscovery discovery;

		for(;;) {
			/* accept connection */
			boost::asio::io_service io_service;
			tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v4(), SERVER_PORT));

			tcp::socket socket(io_service);
			acceptor.accept(socket);

			string remote_address = socket.remote_endpoint().address().to_string();
			printf("Connected to remote client at: %s\n", remote_address.c_str());

			DeviceServer server(this, socket);
			server.listen();

			printf("Disconnected.\n");
		}
	}
	catch(exception& e) {
		fprintf(stderr, "Network server exception: %s\n", e.what());
	}
}

#endif

CCL_NAMESPACE_END

