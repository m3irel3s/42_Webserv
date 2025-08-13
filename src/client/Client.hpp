/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: meferraz <meferraz@student.42porto.pt>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/04 17:01:17 by jmeirele          #+#    #+#             */
/*   Updated: 2025/08/13 11:15:13 by meferraz         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "../../inc/webserv.hpp"
#include "../config/ServerConfig.hpp"
#include "../http/Request.hpp"
#include "../http/Response.hpp"
#include "../http/RequestHandler.hpp"
#include "../utils/Logger.hpp"

class Client
{
	public:
		Client(int fd, const struct sockaddr_in& addr, const ServerConfig &config);
		~Client();

		bool handleClientRequest();
		bool handleClientResponse();
		void closeClient();

		int getFd() const;
		const std::string& getClientAddress() const;
		bool isClientClosed() const;

	private:
		int _fd;
		bool _closed;
		std::string _readBuffer;
		std::string _writeBuffer;
		std::string _clientAddress;

		Request *_request;
		Response _response;
		const ServerConfig &_config;

		bool _hasCompleteRequest() const;
		size_t _getContentLength() const;
};
