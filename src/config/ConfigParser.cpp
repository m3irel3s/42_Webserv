/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: meferraz <meferraz@student.42porto.pt>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/06 16:50:07 by meferraz          #+#    #+#             */
/*   Updated: 2025/08/13 11:28:30 by meferraz         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ConfigParser.hpp"

ConfigParser::ConfigParser(const std::string& path) : _path(path)
{
	_initHandlers();
}

std::vector<ServerConfig> ConfigParser::parse()
{
	std::string content = _readFile();
	std::istringstream stream(content);
	return _parseConfig(stream);
}

std::string ConfigParser::_readFile()
{
	std::ifstream file(_path.c_str());
	if (!file.is_open())
	{
		throw std::runtime_error("Failed to open config file: " + _path);
	}

	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

void ConfigParser::_initHandlers()
{
	_serverHandlers["listen"] = &ConfigParser::_handleListen;
	_serverHandlers["server_name"] = &ConfigParser::_handleServerName;
	_serverHandlers["root"] = &ConfigParser::_handleRoot;
	_serverHandlers["index"] = &ConfigParser::_handleIndex;
	_serverHandlers["error_page"] = &ConfigParser::_handleErrorPage;
	_serverHandlers["client_max_body_size"] =
		&ConfigParser::_handleClientMaxBodySize;
	_serverHandlers["autoindex"] = &ConfigParser::_handleServerAutoIndex;

	_locationHandlers["root"] = &ConfigParser::_handleLocRoot;
	_locationHandlers["index"] = &ConfigParser::_handleLocIndex;
	_locationHandlers["autoindex"] = &ConfigParser::_handleAutoIndex;
	_locationHandlers["allow_methods"] = &ConfigParser::_handleAllowMethods;
	_locationHandlers["return"] = &ConfigParser::_handleLocReturn;
	_locationHandlers["cgi"] = &ConfigParser::_handleCgi;
}

std::string ConfigParser::trim(const std::string &s)
{
	size_t first = s.find_first_not_of(" \t\r\n;");
	size_t last = s.find_last_not_of(" \t\r\n;");
	if (first == std::string::npos || last == std::string::npos)
		return "";
	return s.substr(first, last - first + 1);
}

std::string ConfigParser::intToString(int v)
{
	std::ostringstream oss;
	oss << v;
	return oss.str();
}

void ConfigParser::_throwError(int lineNum, const std::string& msg) const
{
	throw std::runtime_error("Line " + intToString(lineNum) + ": " + msg);
}

std::vector<ServerConfig> ConfigParser::_parseConfig(std::istringstream& stream)
{
	std::map<ServerKey, ServerConfig> serverMap;
	std::string line;
	int lineNum = 0;

	while (std::getline(stream, line))
	{
		++lineNum;
		std::string trimmed = trim(line);
		if (trimmed.empty() || trimmed[0] == '#')
			continue;

		std::istringstream iss(trimmed);
		std::string token;
		iss >> token;
		if (token == "server")
		{
			_parseServerBlock(trimmed, lineNum, stream, serverMap);
		}
		else
		{
			_throwError(lineNum, "Unexpected token '" + token + "'");
		}
	}

	std::vector<ServerConfig> servers;
	for (std::map<ServerKey, ServerConfig>::iterator it = serverMap.begin();
		it != serverMap.end(); ++it)
	{
		servers.push_back(it->second);
	}
	return servers;
}

bool ConfigParser::_findOpeningBrace(const std::string& firstLine, int& lineNum,
									std::istringstream& stream,
									const std::string& context)
{
	std::istringstream iline(firstLine);
	std::string token;

	// Skip past the directive name and check for brace on same line
	while (iline >> token && token != "{")
		;

	if (token == "{")
		return true;

	// Look for brace on next non-empty, non-comment line
	std::string nextLine;
	while (std::getline(stream, nextLine))
	{
		++lineNum;
		std::string trimmed = trim(nextLine);

		if (trimmed.empty() || trimmed[0] == '#')
			continue;

		if (trimmed == "{")
			return true;

		_throwError(lineNum, "Expected '{' after " + context + ", found: " + trimmed);
	}

	_throwError(lineNum, "Expected '{' after " + context);
	return false;
}

void ConfigParser::_parseServerBlock(const std::string& firstLine, int& lineNum,
									std::istringstream& stream,
									std::map<ServerKey, ServerConfig>& servers)
{
	if (!_findOpeningBrace(firstLine, lineNum, stream, "server"))
		return;

	ServerConfig currentConfig;
	int braceCount = 1;
	std::string rawLine;

	while (braceCount > 0 && std::getline(stream, rawLine))
	{
		++lineNum;
		std::string line = trim(rawLine);
		if (line.empty() || line[0] == '#')
			continue;

		if (line == "{")
			++braceCount;
		else if (line == "}")
		{
			--braceCount;
			if (braceCount == 0)
			{
				_finalizeServerBlock(lineNum, currentConfig, servers);
				return;
			}
		}
		else
		{
			_processServerLine(line, lineNum, stream, currentConfig);
		}
	}

	if (braceCount != 0)
		_throwError(lineNum, "Unexpected end of server block");
}

void ConfigParser::_finalizeServerBlock(int lineNum, ServerConfig& currentConfig,
										std::map<ServerKey, ServerConfig>& servers)
{
	_validateServerBlock(currentConfig, lineNum);
	if (currentConfig.getListens().empty())
		_throwError(lineNum, "Missing 'listen' directive");

	ServerKey key;
	const ListenConfig& firstListen =
		currentConfig.getListens().begin()->second;

	key.host = firstListen.getIp();
	key.port = firstListen.getPort();

	std::vector<std::string> serverNames = currentConfig.getServerNames();
	key.names.insert(serverNames.begin(), serverNames.end());

	if (servers.find(key) != servers.end())
		_throwError(lineNum, "Duplicate server");

	servers[key] = currentConfig;
}

void ConfigParser::_processServerLine(const std::string& line, int lineNum,
									std::istringstream& stream,
									ServerConfig& currentConfig)
{
	std::istringstream ls(line);
	std::string key;
	ls >> key;
	std::string remainder;
	std::getline(ls, remainder);
	remainder = trim(remainder);

	ServerHandlerMap::iterator it = _serverHandlers.find(key);
	if (it != _serverHandlers.end())
	{
		(this->*(it->second))(remainder, currentConfig, lineNum);
	}
	else if (key == "location")
	{
		_parseLocationBlock(line, lineNum, stream, currentConfig);
	}
	else
	{
		_throwError(lineNum, "Unknown directive '" + key + "' in server block");
	}
}

void ConfigParser::_parseLocationBlock(const std::string& firstLine, int& lineNum,
										std::istringstream& stream,
										ServerConfig& currentConfig)
{
	std::istringstream iline(firstLine);
	std::string keyword, path, brace;

	iline >> keyword;
	iline >> path;
	iline >> brace;

	if (path.empty())
		_throwError(lineNum, "Location directive requires a path argument");

	// Use the enhanced brace finding method
	if (!_findOpeningBrace(firstLine, lineNum, stream, "location " + path))
		return;

	LocationConfig loc;
	loc.setPath(path);
	int braceCount = 1;
	std::string rawLine;

	while (braceCount > 0 && std::getline(stream, rawLine))
	{
		++lineNum;
		std::string line = trim(rawLine);
		if (line.empty() || line[0] == '#')
			continue;

		if (line == "{")
			++braceCount;
		else if (line == "}")
		{
			--braceCount;
			if (braceCount == 0)
			{
				currentConfig.addLocation(loc);
				return;
			}
		}
		else
		{
			_processLocationLine(line, lineNum, stream, loc);
		}
	}

	if (braceCount != 0)
		_throwError(lineNum, "Unexpected end of location block");
}

void ConfigParser::_processLocationLine(const std::string& line, int lineNum,
										std::istringstream& stream,
										LocationConfig& loc)
{
	(void)stream;
	std::istringstream ls(line);
	std::string key;
	ls >> key;
	std::string remainder;
	std::getline(ls, remainder);
	remainder = trim(remainder);

	LocationHandlerMap::iterator it = _locationHandlers.find(key);
	if (it != _locationHandlers.end())
	{
		(this->*(it->second))(remainder, loc, lineNum);
	}
	else
	{
		_throwError(lineNum, "Unknown directive '" + key + "' in location block");
	}
}

void ConfigParser::_handleListen(const std::string& args,
								ServerConfig& cfg, int /* lineNum */)
{
	cfg.addListen(trim(args));
}

void ConfigParser::_handleServerName(const std::string& args,
									ServerConfig& cfg, int lineNum)
{
	std::istringstream ss(args);
	std::string name;

	if (ss >> name)
		cfg.addServerName(name);
	else
		_throwError(lineNum, "Invalid server_name syntax");
}

void ConfigParser::_handleRoot(const std::string& args,
							ServerConfig& cfg, int /* lineNum */)
{
	cfg.setRoot(args);
}

void ConfigParser::_handleIndex(const std::string& args,
								ServerConfig& cfg, int lineNum)
{
	if (args.empty())
		_throwError(lineNum, "Index directive requires at least one argument");

	std::istringstream ss(args);
	std::vector<std::string> indexes;
	std::string index;
	while (ss >> index)
		indexes.push_back(index);
	cfg.setIndex(indexes);
}

void ConfigParser::_handleErrorPage(const std::string& args,
									ServerConfig& cfg, int lineNum)
{
	std::istringstream ss(args);
	int code;
	std::string path;
	ss >> code >> path;
	if (code < 100 || code > 599)
		_throwError(lineNum, "Invalid error code: must be between 100 and 599");
	if (ss.fail() || !ss.eof())
		_throwError(lineNum, "Invalid error_page syntax");
	cfg.setErrorPage(code, path);
}

void ConfigParser::_handleServerAutoIndex(const std::string &args,
									ServerConfig &cfg, int lineNum)
{
	if (args == "on")
		cfg.setServerAutoIndex(true);
	else if (args == "off")
		cfg.setServerAutoIndex(false);
	else
		_throwError(lineNum, "Invalid autoindex value in server");
}

void ConfigParser::_handleClientMaxBodySize(const std::string& args,
									ServerConfig& cfg, int lineNum)
{
	std::istringstream ss(args);
	size_t sz;
	ss >> sz;
	if (ss.fail())
		_throwError(lineNum, "Invalid client_max_body_size");
	cfg.setClientMaxBodySize(sz);
}

void ConfigParser::_handleLocRoot(const std::string& args,
								LocationConfig& loc, int lineNum)
{
	if (args.empty())
		_throwError(lineNum, "root directive requires a path argument");
	loc.setRoot(args);
}

void ConfigParser::_handleLocIndex(const std::string& args,
								LocationConfig& loc, int lineNum)
{
	if (args.empty())
		_throwError(lineNum, "Index directive requires at least one argument");
	loc.addIndex(args);
}

void ConfigParser::_handleAutoIndex(const std::string& args,
									LocationConfig& loc, int lineNum)
{
	if (args == "on")
		loc.setAutoIndex(true);
	else if (args == "off")
		loc.setAutoIndex(false);
	else
		_throwError(lineNum, "Invalid autoindex value");
}

void ConfigParser::_handleAllowMethods(const std::string& args,
									LocationConfig& loc, int lineNum)
{
	if (args.empty())
		_throwError(lineNum, "allow_methods directive requires at least one method");

	std::istringstream ss(args);
	std::string m;
	while (ss >> m)
		loc.addAllowedMethod(m);
}

void ConfigParser::_handleLocReturn(const std::string& args,
									LocationConfig& loc, int lineNum)
{
	std::istringstream ss(args);
	int code;
	std::string target;
	ss >> code >> target;
	if (ss.fail() || !ss.eof())
		_throwError(lineNum, "Invalid return syntax");
	loc.addRedirect(code, target);
}

void ConfigParser::_handleCgi(const std::string& args,
							LocationConfig& loc, int lineNum)
{
	std::istringstream ss(args);
	std::string ext, cgiPath;
	ss >> ext >> cgiPath;

	// ONLY ALLOW PHP
	if (ext != ".php") {
		_throwError(lineNum, "Only PHP CGI is supported: use '.php'");
	}

	if (ext.empty() || ext[0] != '.')
		_throwError(lineNum, "Invalid CGI extension (must start with '.')");

	loc.addCgi(ext, cgiPath);
}


void ConfigParser::_validateServerBlock(const ServerConfig& config, int lineNum)
{
	if (config.getListens().empty()) {
		_throwError(lineNum, "Server block must have at least one 'listen' directive");
	}

	if (config.getServerRoot().empty()) {
		_throwError(lineNum, "Server block must have a 'root' directive");
	}

	// Validate paths exist
	struct stat statBuf;
	if (stat(config.getServerRoot().c_str(), &statBuf) != 0) {
		_throwError(lineNum, "Root directory does not exist: " + config.getServerRoot());
	}
}
