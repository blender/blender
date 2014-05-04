/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
 */

#include "device.h"
#include "device_intern.h"
#include "device_network.h"

#include "util_foreach.h"

#if defined(WITH_NETWORK)

CCL_NAMESPACE_BEGIN

typedef map<device_ptr, device_ptr> PtrMap;
typedef vector<uint8_t> DataVector;
typedef map<device_ptr, DataVector> DataMap;

/* tile list */
typedef vector<RenderTile> TileList;

/* search a list of tiles and find the one that matches the passed render tile */
static TileList::iterator tile_list_find(TileList& tile_list, RenderTile& tile)
{
	for(TileList::iterator it = tile_list.begin(); it != tile_list.end(); ++it)
		if(tile.x == it->x && tile.y == it->y && tile.start_sample == it->start_sample)
			return it;
	return tile_list.end();
}

class NetworkDevice : public Device
{
public:
	boost::asio::io_service io_service;
	tcp::socket socket;
	device_ptr mem_counter;
	DeviceTask the_task; /* todo: handle multiple tasks */

	thread_mutex rpc_lock;

	NetworkDevice(DeviceInfo& info, Stats &stats, const char *address)
	: Device(info, stats, true), socket(io_service)
	{
		error_func = NetworkError();
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
			error_func.network_error(error.message());

		mem_counter = 0;
	}

	~NetworkDevice()
	{
		RPCSend snd(socket, &error_func, "stop");
		snd.write();
	}

	void mem_alloc(device_memory& mem, MemoryType type)
	{
		thread_scoped_lock lock(rpc_lock);

		mem.device_pointer = ++mem_counter;

		RPCSend snd(socket, &error_func, "mem_alloc");

		snd.add(mem);
		snd.add(type);
		snd.write();
	}

	void mem_copy_to(device_memory& mem)
	{
		thread_scoped_lock lock(rpc_lock);

		RPCSend snd(socket, &error_func, "mem_copy_to");

		snd.add(mem);
		snd.write();
		snd.write_buffer((void*)mem.data_pointer, mem.memory_size());
	}

	void mem_copy_from(device_memory& mem, int y, int w, int h, int elem)
	{
		thread_scoped_lock lock(rpc_lock);

		size_t data_size = mem.memory_size();

		RPCSend snd(socket, &error_func, "mem_copy_from");

		snd.add(mem);
		snd.add(y);
		snd.add(w);
		snd.add(h);
		snd.add(elem);
		snd.write();

		RPCReceive rcv(socket, &error_func);
		rcv.read_buffer((void*)mem.data_pointer, data_size);
	}

	void mem_zero(device_memory& mem)
	{
		thread_scoped_lock lock(rpc_lock);

		RPCSend snd(socket, &error_func, "mem_zero");

		snd.add(mem);
		snd.write();
	}

	void mem_free(device_memory& mem)
	{
		if(mem.device_pointer) {
			thread_scoped_lock lock(rpc_lock);

			RPCSend snd(socket, &error_func, "mem_free");

			snd.add(mem);
			snd.write();

			mem.device_pointer = 0;
		}
	}

	void const_copy_to(const char *name, void *host, size_t size)
	{
		thread_scoped_lock lock(rpc_lock);

		RPCSend snd(socket, &error_func, "const_copy_to");

		string name_string(name);

		snd.add(name_string);
		snd.add(size);
		snd.write();
		snd.write_buffer(host, size);
	}

	void tex_alloc(const char *name, device_memory& mem, InterpolationType interpolation, bool periodic)
	{
		thread_scoped_lock lock(rpc_lock);

		mem.device_pointer = ++mem_counter;

		RPCSend snd(socket, &error_func, "tex_alloc");

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
			thread_scoped_lock lock(rpc_lock);

			RPCSend snd(socket, &error_func, "tex_free");

			snd.add(mem);
			snd.write();

			mem.device_pointer = 0;
		}
	}

	bool load_kernels(bool experimental)
	{
		if(error_func.have_error())
			return false;

		thread_scoped_lock lock(rpc_lock);

		RPCSend snd(socket, &error_func, "load_kernels");
		snd.add(experimental);
		snd.write();

		bool result;
		RPCReceive rcv(socket, &error_func);
		rcv.read(result);

		return result;
	}

	void task_add(DeviceTask& task)
	{
		thread_scoped_lock lock(rpc_lock);

		the_task = task;

		RPCSend snd(socket, &error_func, "task_add");
		snd.add(task);
		snd.write();
	}

	void task_wait()
	{
		thread_scoped_lock lock(rpc_lock);

		RPCSend snd(socket, &error_func, "task_wait");
		snd.write();

		lock.unlock();

		TileList the_tiles;

		/* todo: run this threaded for connecting to multiple clients */
		for(;;) {
			if(error_func.have_error())
				break;

			RenderTile tile;

			lock.lock();
			RPCReceive rcv(socket, &error_func);

			if(rcv.name == "acquire_tile") {
				lock.unlock();

				/* todo: watch out for recursive calls! */
				if(the_task.acquire_tile(this, tile)) { /* write return as bool */
					the_tiles.push_back(tile);

					lock.lock();
					RPCSend snd(socket, &error_func, "acquire_tile");
					snd.add(tile);
					snd.write();
					lock.unlock();
				}
				else {
					lock.lock();
					RPCSend snd(socket, &error_func, "acquire_tile_none");
					snd.write();
					lock.unlock();
				}
			}
			else if(rcv.name == "release_tile") {
				rcv.read(tile);
				lock.unlock();

				TileList::iterator it = tile_list_find(the_tiles, tile);
				if (it != the_tiles.end()) {
					tile.buffers = it->buffers;
					the_tiles.erase(it);
				}

				assert(tile.buffers != NULL);

				the_task.release_tile(tile);

				lock.lock();
				RPCSend snd(socket, &error_func, "release_tile");
				snd.write();
				lock.unlock();
			}
			else if(rcv.name == "task_wait_done") {
				lock.unlock();
				break;
			}
			else
				lock.unlock();
		}
	}

	void task_cancel()
	{
		thread_scoped_lock lock(rpc_lock);
		RPCSend snd(socket, &error_func, "task_cancel");
		snd.write();
	}

private:
	NetworkError error_func;
};

Device *device_network_create(DeviceInfo& info, Stats &stats, const char *address)
{
	return new NetworkDevice(info, stats, address);
}

void device_network_info(vector<DeviceInfo>& devices)
{
	DeviceInfo info;

	info.type = DEVICE_NETWORK;
	info.description = "Network Device";
	info.id = "NETWORK";
	info.num = 0;
	info.advanced_shading = true; /* todo: get this info from device */
	info.pack_images = false;

	devices.push_back(info);
}

class DeviceServer {
public:
	thread_mutex rpc_lock;

	void network_error(const string &message) {
		error_func.network_error(message);
	}

	bool have_error() { return error_func.have_error(); }

	DeviceServer(Device *device_, tcp::socket& socket_)
	: device(device_), socket(socket_), stop(false), blocked_waiting(false)
	{
		error_func = NetworkError();
	}

	void listen()
	{
		/* receive remote function calls */
		for(;;) {
			listen_step();

			if(stop)
				break;
		}
	}

protected:
	void listen_step()
	{
		thread_scoped_lock lock(rpc_lock);
		RPCReceive rcv(socket, &error_func);

		if(rcv.name == "stop")
			stop = true;
		else
			process(rcv, lock);
	}

	/* create a memory buffer for a device buffer and insert it into mem_data */
	DataVector &data_vector_insert(device_ptr client_pointer, size_t data_size)
	{
		/* create a new DataVector and insert it into mem_data */
		pair<DataMap::iterator,bool> data_ins = mem_data.insert(
		        DataMap::value_type(client_pointer, DataVector()));

		/* make sure it was a unique insertion */
		assert(data_ins.second);

		/* get a reference to the inserted vector */
		DataVector &data_v = data_ins.first->second;

		/* size the vector */
		data_v.resize(data_size);

		return data_v;
	}

	DataVector &data_vector_find(device_ptr client_pointer)
	{
		DataMap::iterator i = mem_data.find(client_pointer);
		assert(i != mem_data.end());
		return i->second;
	}

	/* setup mapping and reverse mapping of client_pointer<->real_pointer */
	void pointer_mapping_insert(device_ptr client_pointer, device_ptr real_pointer)
	{
		pair<PtrMap::iterator,bool> mapins;

		/* insert mapping from client pointer to our real device pointer */
		mapins = ptr_map.insert(PtrMap::value_type(client_pointer, real_pointer));
		assert(mapins.second);

		/* insert reverse mapping from real our device pointer to client pointer */
		mapins = ptr_imap.insert(PtrMap::value_type(real_pointer, client_pointer));
		assert(mapins.second);
	}

	device_ptr device_ptr_from_client_pointer(device_ptr client_pointer)
	{
		PtrMap::iterator i = ptr_map.find(client_pointer);
		assert(i != ptr_map.end());
		return i->second;
	}

	device_ptr device_ptr_from_client_pointer_erase(device_ptr client_pointer)
	{
		PtrMap::iterator i = ptr_map.find(client_pointer);
		assert(i != ptr_map.end());

		device_ptr result = i->second;

		/* erase the mapping */
		ptr_map.erase(i);

		/* erase the reverse mapping */
		PtrMap::iterator irev = ptr_imap.find(result);
		assert(irev != ptr_imap.end());
		ptr_imap.erase(irev);

		/* erase the data vector */
		DataMap::iterator idata = mem_data.find(client_pointer);
		assert(idata != mem_data.end());
		mem_data.erase(idata);

		return result;
	}

	/* note that the lock must be already acquired upon entry.
	 * This is necessary because the caller often peeks at
	 * the header and delegates control to here when it doesn't
	 * specifically handle the current RPC.
	 * The lock must be unlocked before returning */
	void process(RPCReceive& rcv, thread_scoped_lock &lock)
	{
		if(rcv.name == "mem_alloc") {
			MemoryType type;
			network_device_memory mem;
			device_ptr client_pointer;

			rcv.read(mem);
			rcv.read(type);

			lock.unlock();

			client_pointer = mem.device_pointer;

			/* create a memory buffer for the device buffer */
			size_t data_size = mem.memory_size();
			DataVector &data_v = data_vector_insert(client_pointer, data_size);

			if(data_size)
				mem.data_pointer = (device_ptr)&(data_v[0]);
			else
				mem.data_pointer = 0;

			/* perform the allocation on the actual device */
			device->mem_alloc(mem, type);

			/* store a mapping to/from client_pointer and real device pointer */
			pointer_mapping_insert(client_pointer, mem.device_pointer);
		}
		else if(rcv.name == "mem_copy_to") {
			network_device_memory mem;

			rcv.read(mem);
			lock.unlock();

			device_ptr client_pointer = mem.device_pointer;

			DataVector &data_v = data_vector_find(client_pointer);

			size_t data_size = mem.memory_size();

			/* get pointer to memory buffer	for device buffer */
			mem.data_pointer = (device_ptr)&data_v[0];

			/* copy data from network into memory buffer */
			rcv.read_buffer((uint8_t*)mem.data_pointer, data_size);

			/* translate the client pointer to a real device pointer */
			mem.device_pointer = device_ptr_from_client_pointer(client_pointer);

			/* copy the data from the memory buffer to the device buffer */
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

			device_ptr client_pointer = mem.device_pointer;
			mem.device_pointer = device_ptr_from_client_pointer(client_pointer);

			DataVector &data_v = data_vector_find(client_pointer);

			mem.data_pointer = (device_ptr)&(data_v[0]);

			device->mem_copy_from(mem, y, w, h, elem);

			size_t data_size = mem.memory_size();

			RPCSend snd(socket, &error_func, "mem_copy_from");
			snd.write();
			snd.write_buffer((uint8_t*)mem.data_pointer, data_size);
			lock.unlock();
		}
		else if(rcv.name == "mem_zero") {
			network_device_memory mem;
			
			rcv.read(mem);
			lock.unlock();

			device_ptr client_pointer = mem.device_pointer;
			mem.device_pointer = device_ptr_from_client_pointer(client_pointer);

			DataVector &data_v = data_vector_find(client_pointer);

			mem.data_pointer = (device_ptr)&(data_v[0]);

			device->mem_zero(mem);
		}
		else if(rcv.name == "mem_free") {
			network_device_memory mem;
			device_ptr client_pointer;

			rcv.read(mem);
			lock.unlock();

			client_pointer = mem.device_pointer;

			mem.device_pointer = device_ptr_from_client_pointer_erase(client_pointer);

			device->mem_free(mem);
		}
		else if(rcv.name == "const_copy_to") {
			string name_string;
			size_t size;

			rcv.read(name_string);
			rcv.read(size);

			vector<char> host_vector(size);
			rcv.read_buffer(&host_vector[0], size);
			lock.unlock();

			device->const_copy_to(name_string.c_str(), &host_vector[0], size);
		}
		else if(rcv.name == "tex_alloc") {
			network_device_memory mem;
			string name;
			InterpolationType interpolation;
			bool periodic;
			device_ptr client_pointer;

			rcv.read(name);
			rcv.read(mem);
			rcv.read(interpolation);
			rcv.read(periodic);
			lock.unlock();

			client_pointer = mem.device_pointer;

			size_t data_size = mem.memory_size();

			DataVector &data_v = data_vector_insert(client_pointer, data_size);

			if(data_size)
				mem.data_pointer = (device_ptr)&(data_v[0]);
			else
				mem.data_pointer = 0;

			rcv.read_buffer((uint8_t*)mem.data_pointer, data_size);

			device->tex_alloc(name.c_str(), mem, interpolation, periodic);

			pointer_mapping_insert(client_pointer, mem.device_pointer);
		}
		else if(rcv.name == "tex_free") {
			network_device_memory mem;
			device_ptr client_pointer;

			rcv.read(mem);
			lock.unlock();

			client_pointer = mem.device_pointer;

			mem.device_pointer = device_ptr_from_client_pointer_erase(client_pointer);

			device->tex_free(mem);
		}
		else if(rcv.name == "load_kernels") {
			bool experimental;
			rcv.read(experimental);

			bool result;
			result = device->load_kernels(experimental);
			RPCSend snd(socket, &error_func, "load_kernels");
			snd.add(result);
			snd.write();
			lock.unlock();
		}
		else if(rcv.name == "task_add") {
			DeviceTask task;

			rcv.read(task);
			lock.unlock();

			if(task.buffer)
				task.buffer = device_ptr_from_client_pointer(task.buffer);

			if(task.rgba_half)
				task.rgba_half = device_ptr_from_client_pointer(task.rgba_half);

			if(task.rgba_byte)
				task.rgba_byte = device_ptr_from_client_pointer(task.rgba_byte);

			if(task.shader_input)
				task.shader_input = device_ptr_from_client_pointer(task.shader_input);

			if(task.shader_output)
				task.shader_output = device_ptr_from_client_pointer(task.shader_output);


			task.acquire_tile = function_bind(&DeviceServer::task_acquire_tile, this, _1, _2);
			task.release_tile = function_bind(&DeviceServer::task_release_tile, this, _1);
			task.update_progress_sample = function_bind(&DeviceServer::task_update_progress_sample, this);
			task.update_tile_sample = function_bind(&DeviceServer::task_update_tile_sample, this, _1);
			task.get_cancel = function_bind(&DeviceServer::task_get_cancel, this);

			device->task_add(task);
		}
		else if(rcv.name == "task_wait") {
			lock.unlock();

			blocked_waiting = true;
			device->task_wait();
			blocked_waiting = false;

			lock.lock();
			RPCSend snd(socket, &error_func, "task_wait_done");
			snd.write();
			lock.unlock();
		}
		else if(rcv.name == "task_cancel") {
			lock.unlock();
			device->task_cancel();
		}
		else if(rcv.name == "acquire_tile") {
			AcquireEntry entry;
			entry.name = rcv.name;
			rcv.read(entry.tile);
			acquire_queue.push_back(entry);
			lock.unlock();
		}
		else if(rcv.name == "acquire_tile_none") {
			AcquireEntry entry;
			entry.name = rcv.name;
			acquire_queue.push_back(entry);
			lock.unlock();
		}
		else if(rcv.name == "release_tile") {
			AcquireEntry entry;
			entry.name = rcv.name;
			acquire_queue.push_back(entry);
			lock.unlock();
		}
		else {
			cout << "Error: unexpected RPC receive call \"" + rcv.name + "\"\n";
			lock.unlock();
		}
	}

	bool task_acquire_tile(Device *device, RenderTile& tile)
	{
		thread_scoped_lock acquire_lock(acquire_mutex);

		bool result = false;

		RPCSend snd(socket, &error_func, "acquire_tile");
		snd.write();

		do {
			if(blocked_waiting)
				listen_step();

			/* todo: avoid busy wait loop */
			thread_scoped_lock lock(rpc_lock);

			if(!acquire_queue.empty()) {
				AcquireEntry entry = acquire_queue.front();
				acquire_queue.pop_front();

				if(entry.name == "acquire_tile") {
					tile = entry.tile;

					if(tile.buffer) tile.buffer = ptr_map[tile.buffer];
					if(tile.rng_state) tile.rng_state = ptr_map[tile.rng_state];

					result = true;
					break;
				}
				else if(entry.name == "acquire_tile_none") {
					break;
				}
				else {
					cout << "Error: unexpected acquire RPC receive call \"" + entry.name + "\"\n";
				}
			}
		} while(acquire_queue.empty() && !stop && !have_error());

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

		{
			thread_scoped_lock lock(rpc_lock);
			RPCSend snd(socket, &error_func, "release_tile");
			snd.add(tile);
			snd.write();
			lock.unlock();
		}

		do {
			if(blocked_waiting)
				listen_step();

			/* todo: avoid busy wait loop */
			thread_scoped_lock lock(rpc_lock);

			if(!acquire_queue.empty()) {
				AcquireEntry entry = acquire_queue.front();
				acquire_queue.pop_front();

				if(entry.name == "release_tile") {
					lock.unlock();
					break;
				}
				else {
					cout << "Error: unexpected release RPC receive call \"" + entry.name + "\"\n";
				}
			}
		} while(acquire_queue.empty() && !stop);
	}

	bool task_get_cancel()
	{
		return false;
	}

	/* properties */
	Device *device;
	tcp::socket& socket;

	/* mapping of remote to local pointer */
	PtrMap ptr_map;
	PtrMap ptr_imap;
	DataMap mem_data;

	struct AcquireEntry {
		string name;
		RenderTile tile;
	};

	thread_mutex acquire_mutex;
	list<AcquireEntry> acquire_queue;

	bool stop;
	bool blocked_waiting;
private:
	NetworkError error_func;

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

CCL_NAMESPACE_END

#endif


