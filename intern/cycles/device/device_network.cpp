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

	NetworkDevice(const char *address)
	: socket(io_service)
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
	}

	~NetworkDevice()
	{
	}

	bool support_full_kernel()
	{
		return false;
	}

	string description()
	{
		RPCSend snd(socket, "description");
		snd.write();

		RPCReceive rcv(socket);
		string desc_string;

		*rcv.archive & desc_string;

		return desc_string + " (remote)";
	}

	void mem_alloc(device_memory& mem, MemoryType type)
	{
#if 0
		RPCSend snd(socket, "mem_alloc");

		snd.archive & size & type;
		snd.write();

		RPCReceive rcv(socket);

		device_ptr mem;
		*rcv.archive & mem;

		return mem;
#endif
	}

	void mem_copy_to(device_memory& mem)
	{
#if 0
		RPCSend snd(socket, "mem_copy_to");

		snd.archive & mem & size;
		snd.write();
		snd.write_buffer(host, size);
#endif
	}

	void mem_copy_from(device_memory& mem, size_t offset, size_t size)
	{
#if 0
		RPCSend snd(socket, "mem_copy_from");

		snd.archive & mem & offset & size;
		snd.write();

		RPCReceive rcv(socket);
		rcv.read_buffer(host, size);
#endif
	}

	void mem_zero(device_memory& mem)
	{
#if 0
		RPCSend snd(socket, "mem_zero");

		snd.archive & mem & size;
		snd.write();
#endif
	}

	void mem_free(device_memory& mem)
	{
#if 0
		if(mem) {
			RPCSend snd(socket, "mem_free");

			snd.archive & mem;
			snd.write();
		}
#endif
	}

	void const_copy_to(const char *name, void *host, size_t size)
	{
		RPCSend snd(socket, "const_copy_to");

		string name_string(name);

		snd.archive & name_string & size;
		snd.write();
		snd.write_buffer(host, size);
	}

	void tex_alloc(const char *name, device_memory& mem, bool interpolation, bool periodic)
	{
#if 0
		RPCSend snd(socket, "tex_alloc");

		string name_string(name);

		snd.archive & name_string & width & height & datatype & components & interpolation;
		snd.write();

		size_t size = width*height*components*datatype_size(datatype);
		snd.write_buffer(host, size);

		RPCReceive rcv(socket);

		device_ptr mem;
		*rcv.archive & mem;

		return mem;
#endif
	}

	void tex_free(device_memory& mem)
	{
#if 0
		if(mem) {
			RPCSend snd(socket, "tex_free");

			snd.archive & mem;
			snd.write();
		}
#endif
	}

	void path_trace(int x, int y, int w, int h, device_ptr buffer, device_ptr rng_state, int sample)
	{
#if 0
		RPCSend snd(socket, "path_trace");

		snd.archive & x & y & w & h & buffer & rng_state & sample;
		snd.write();
#endif
	}

	void tonemap(int x, int y, int w, int h, device_ptr rgba, device_ptr buffer, int sample, int resolution)
	{
#if 0
		RPCSend snd(socket, "tonemap");

		snd.archive & x & y & w & h & rgba & buffer & sample & resolution;
		snd.write();
#endif
	}

	void task_add(DeviceTask& task)
	{
		if(task.type == DeviceTask::TONEMAP)
			tonemap(task.x, task.y, task.w, task.h, task.rgba, task.buffer, task.sample, task.resolution);
		else if(task.type == DeviceTask::PATH_TRACE)
			path_trace(task.x, task.y, task.w, task.h, task.buffer, task.rng_state, task.sample);
	}

	void task_wait()
	{
	}

	void task_cancel()
	{
	}
};

Device *device_network_create(DeviceInfo& info, const char *address)
{
	return new NetworkDevice(address);
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

void Device::server_run()
{
	try
	{
		/* starts thread that responds to discovery requests */
		ServerDiscovery discovery;

		for(;;)
		{

			/* accept connection */
			boost::asio::io_service io_service;
			tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v4(), SERVER_PORT));

			tcp::socket socket(io_service);
			acceptor.accept(socket);

			/* receive remote function calls */
			for(;;) {
				RPCReceive rcv(socket);

				if(rcv.name == "description") {
					string desc = description();

					RPCSend snd(socket);
					snd.archive & desc;
					snd.write();
				}
				else if(rcv.name == "mem_alloc") {
#if 0
					MemoryType type;
					size_t size;
					device_ptr mem;

					*rcv.archive & size & type;
					mem = mem_alloc(size, type);

					RPCSend snd(socket);
					snd.archive & mem;
					snd.write();
#endif
				}
				else if(rcv.name == "mem_copy_to") {
#if 0
					device_ptr mem;
					size_t size;

					*rcv.archive & mem & size;

					vector<char> host_vector(size);
					rcv.read_buffer(&host_vector[0], size);

					mem_copy_to(mem, &host_vector[0], size);
#endif
				}
				else if(rcv.name == "mem_copy_from") {
#if 0
					device_ptr mem;
					size_t offset, size;

					*rcv.archive & mem & offset & size;

					vector<char> host_vector(size);

					mem_copy_from(&host_vector[0], mem, offset, size);

					RPCSend snd(socket);
					snd.write();
					snd.write_buffer(&host_vector[0], size);
#endif
				}
				else if(rcv.name == "mem_zero") {
#if 0
					device_ptr mem;
					size_t size;

					*rcv.archive & mem & size;
					mem_zero(mem, size);
#endif
				}
				else if(rcv.name == "mem_free") {
#if 0
					device_ptr mem;

					*rcv.archive & mem;
					mem_free(mem);
#endif
				}
				else if(rcv.name == "const_copy_to") {
					string name_string;
					size_t size;

					*rcv.archive & name_string & size;

					vector<char> host_vector(size);
					rcv.read_buffer(&host_vector[0], size);

					const_copy_to(name_string.c_str(), &host_vector[0], size);
				}
				else if(rcv.name == "tex_alloc") {
#if 0
					string name_string;
					DataType datatype;
					device_ptr mem;
					size_t width, height;
					int components;
					bool interpolation;

					*rcv.archive & name_string & width & height & datatype & components & interpolation;

					size_t size = width*height*components*datatype_size(datatype);

					vector<char> host_vector(size);
					rcv.read_buffer(&host_vector[0], size);

					mem = tex_alloc(name_string.c_str(), &host_vector[0], width, height, datatype, components, interpolation);

					RPCSend snd(socket);
					snd.archive & mem;
					snd.write();
#endif
				}
				else if(rcv.name == "tex_free") {
#if 0
					device_ptr mem;

					*rcv.archive & mem;
					tex_free(mem);
#endif
				}
				else if(rcv.name == "path_trace") {
#if 0
					device_ptr buffer, rng_state;
					int x, y, w, h;
					int sample;

					*rcv.archive & x & y & w & h & buffer & rng_state & sample;
					path_trace(x, y, w, h, buffer, rng_state, sample);
#endif
				}
				else if(rcv.name == "tonemap") {
#if 0
					device_ptr rgba, buffer;
					int x, y, w, h;
					int sample, resolution;

					*rcv.archive & x & y & w & h & rgba & buffer & sample & resolution;
					tonemap(x, y, w, h, rgba, buffer, sample, resolution);
#endif
				}
			}
		}
	}
	catch(exception& e)
	{
		cerr << "Network server exception: " << e.what() << endl;
	}
}

#endif

CCL_NAMESPACE_END

