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

#ifndef __DEVICE_NETWORK_H__
#define __DEVICE_NETWORK_H__

#ifdef WITH_NETWORK

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/thread.hpp>

#include <iostream>

#include "buffers.h"

#include "util_foreach.h"
#include "util_list.h"
#include "util_map.h"
#include "util_string.h"

CCL_NAMESPACE_BEGIN

using std::cout;
using std::cerr;
using std::hex;
using std::setw;
using std::exception;

using boost::asio::ip::tcp;

static const int SERVER_PORT = 5120;
static const int DISCOVER_PORT = 5121;
static const string DISCOVER_REQUEST_MSG = "REQUEST_RENDER_SERVER_IP";
static const string DISCOVER_REPLY_MSG = "REPLY_RENDER_SERVER_IP";

/* Serialization of device memory */

class network_device_memory : public device_memory
{
public:
	network_device_memory() {}
	~network_device_memory() { device_pointer = 0; };

	vector<char> local_data;
};

/* Remote procedure call Send */

class RPCSend {
public:
	RPCSend(tcp::socket& socket_, const string& name_ = "")
	: name(name_), socket(socket_), archive(archive_stream), sent(false)
	{
		archive & name_;
	}

	~RPCSend()
	{
		if(!sent)
			fprintf(stderr, "Error: RPC %s not sent\n", name.c_str());
	}

	void add(const device_memory& mem)
	{
		archive & mem.data_type & mem.data_elements & mem.data_size;
		archive & mem.data_width & mem.data_height & mem.device_pointer;
	}

	template<typename T> void add(const T& data)
	{
		archive & data;
	}

	void add(const DeviceTask& task)
	{
		int type = (int)task.type;

		archive & type & task.x & task.y & task.w & task.h;
		archive & task.rgba & task.buffer & task.sample & task.num_samples;
		archive & task.resolution & task.offset & task.stride;
		archive & task.shader_input & task.shader_output & task.shader_eval_type;
		archive & task.shader_x & task.shader_w;
	}

	void add(const RenderTile& tile)
	{
		archive & tile.x & tile.y & tile.w & tile.h;
		archive & tile.start_sample & tile.num_samples & tile.sample;
		archive & tile.resolution & tile.offset & tile.stride;
		archive & tile.buffer & tile.rng_state & tile.rgba;
	}

	void write()
	{
		boost::system::error_code error;

		/* get string from stream */
		string archive_str = archive_stream.str();

		/* first send fixed size header with size of following data */
		ostringstream header_stream;
		header_stream << setw(8) << hex << archive_str.size();
		string header_str = header_stream.str();

		boost::asio::write(socket,
			boost::asio::buffer(header_str),
			boost::asio::transfer_all(), error);

		if(error.value())
			cout << "Network send error: " << error.message() << "\n";

		/* then send actual data */
		boost::asio::write(socket,
			boost::asio::buffer(archive_str),
			boost::asio::transfer_all(), error);
		
		if(error.value())
			cout << "Network send error: " << error.message() << "\n";

		sent = true;
	}

	void write_buffer(void *buffer, size_t size)
	{
		boost::system::error_code error;

		boost::asio::write(socket,
			boost::asio::buffer(buffer, size),
			boost::asio::transfer_all(), error);
		
		if(error.value())
			cout << "Network send error: " << error.message() << "\n";
	}

protected:
	string name;
	tcp::socket& socket;
	ostringstream archive_stream;
	boost::archive::text_oarchive archive;
	bool sent;
};

/* Remote procedure call Receive */

class RPCReceive {
public:
	RPCReceive(tcp::socket& socket_)
	: socket(socket_), archive_stream(NULL), archive(NULL)
	{
		/* read head with fixed size */
		vector<char> header(8);
		size_t len = boost::asio::read(socket, boost::asio::buffer(header));

		/* verify if we got something */
		if(len == header.size()) {
			/* decode header */
			string header_str(&header[0], header.size());
			istringstream header_stream(header_str);

			size_t data_size;

			if((header_stream >> hex >> data_size)) {
				vector<char> data(data_size);
				size_t len = boost::asio::read(socket, boost::asio::buffer(data));

				if(len == data_size) {
					archive_str = (data.size())? string(&data[0], data.size()): string("");
#if 0
					istringstream archive_stream(archive_str);
					boost::archive::text_iarchive archive(archive_stream);
#endif
					archive_stream = new istringstream(archive_str);
					archive = new boost::archive::text_iarchive(*archive_stream);

					*archive & name;
				}
				else
					cout << "Network receive error: data size doens't match header\n";
			}
			else
				cout << "Network receive error: can't decode data size from header\n";
		}
		else
			cout << "Network receive error: invalid header size\n";
	}

	~RPCReceive()
	{
		delete archive;
		delete archive_stream;
	}

	void read(network_device_memory& mem)
	{
		*archive & mem.data_type & mem.data_elements & mem.data_size;
		*archive & mem.data_width & mem.data_height & mem.device_pointer;

		mem.data_pointer = 0;
	}

	template<typename T> void read(T& data)
	{
		*archive & data;
	}

	void read_buffer(void *buffer, size_t size)
	{
		size_t len = boost::asio::read(socket, boost::asio::buffer(buffer, size));

		if(len != size)
			cout << "Network receive error: buffer size doesn't match expected size\n";
	}

	void read(DeviceTask& task)
	{
		int type;

		*archive & type & task.x & task.y & task.w & task.h;
		*archive & task.rgba & task.buffer & task.sample & task.num_samples;
		*archive & task.resolution & task.offset & task.stride;
		*archive & task.shader_input & task.shader_output & task.shader_eval_type;
		*archive & task.shader_x & task.shader_w;

		task.type = (DeviceTask::Type)type;
	}

	void read(RenderTile& tile)
	{
		*archive & tile.x & tile.y & tile.w & tile.h;
		*archive & tile.start_sample & tile.num_samples & tile.sample;
		*archive & tile.resolution & tile.offset & tile.stride;
		*archive & tile.buffer & tile.rng_state & tile.rgba;

		tile.buffers = NULL;
	}

	string name;

protected:
	tcp::socket& socket;
	string archive_str;
	istringstream *archive_stream;
	boost::archive::text_iarchive *archive;
};

/* Server auto discovery */

class ServerDiscovery {
public:
	ServerDiscovery(bool discover = false)
	: listen_socket(io_service), collect_servers(false)
	{
		/* setup listen socket */
		listen_endpoint.address(boost::asio::ip::address_v4::any());
		listen_endpoint.port(DISCOVER_PORT);

		listen_socket.open(listen_endpoint.protocol());

		boost::asio::socket_base::reuse_address option(true);
		listen_socket.set_option(option);

		listen_socket.bind(listen_endpoint);

		/* setup receive callback */
		async_receive();

		/* start server discovery */
		if(discover) {
			collect_servers = true;
			servers.clear();

			broadcast_message(DISCOVER_REQUEST_MSG);
		}

		/* start thread */
		work = new boost::asio::io_service::work(io_service);
		thread = new boost::thread(boost::bind(&boost::asio::io_service::run, &io_service));
	}

	~ServerDiscovery()
	{
		io_service.stop();
		thread->join();
		delete thread;
		delete work;
	}

	list<string> get_server_list()
	{
		list<string> result;

		mutex.lock();
		result = servers;
		mutex.unlock();

		return result;
	}

private:
	void handle_receive_from(const boost::system::error_code& error, size_t size)
	{
		if(error) {
			cout << "Server discovery receive error: " << error.message() << "\n";
			return;
		}

		if(size > 0) {
			string msg = string(receive_buffer, size);

			/* handle incoming message */
			if(collect_servers) {
				if(msg == DISCOVER_REPLY_MSG) {
					string address = receive_endpoint.address().to_string();

					mutex.lock();

					/* add address if it's not already in the list */
					bool found = false;

					foreach(string& server, servers)
						if(server == address)
							found = true;

					if(!found)
						servers.push_back(address);

					mutex.unlock();
				}
			}
			else {
				/* reply to request */
				if(msg == DISCOVER_REQUEST_MSG)
					broadcast_message(DISCOVER_REPLY_MSG);
			}
		}

		async_receive();
	}

	void async_receive()
	{
		listen_socket.async_receive_from(
			boost::asio::buffer(receive_buffer), receive_endpoint,
			boost::bind(&ServerDiscovery::handle_receive_from, this,
			boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
	}

	void broadcast_message(const string& msg)
	{
		/* setup broadcast socket */
		boost::asio::ip::udp::socket socket(io_service);

		socket.open(boost::asio::ip::udp::v4());

		boost::asio::socket_base::broadcast option(true);
		socket.set_option(option);

		boost::asio::ip::udp::endpoint broadcast_endpoint(
			boost::asio::ip::address::from_string("255.255.255.255"), DISCOVER_PORT);

		/* broadcast message */
		socket.send_to(boost::asio::buffer(msg), broadcast_endpoint);
	}

	/* network service and socket */
	boost::asio::io_service io_service;
	boost::asio::ip::udp::endpoint listen_endpoint;
	boost::asio::ip::udp::socket listen_socket;

	/* threading */
	boost::thread *thread;
	boost::asio::io_service::work *work;
	boost::mutex mutex;

	/* buffer and endpoint for receiving messages */
	char receive_buffer[256];
	boost::asio::ip::udp::endpoint receive_endpoint;

	/* collection of server addresses in list */
	bool collect_servers;
	list<string> servers;
};

CCL_NAMESPACE_END

#endif

#endif /* __DEVICE_NETWORK_H__ */

