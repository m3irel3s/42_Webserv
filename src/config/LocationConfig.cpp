/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   LocationConfig.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: meferraz <meferraz@student.42porto.pt>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/06 16:28:01 by meferraz          #+#    #+#             */
/*   Updated: 2025/08/13 11:10:44 by meferraz         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "LocationConfig.hpp"

LocationConfig::LocationConfig() :
	_path(""),
	_root(""),
	_autoindex(false)
{}

LocationConfig::~LocationConfig() {}

// Validate that the location path is not empty and starts with '/'
void LocationConfig::_validatePath(const std::string& path) const
{
	if (path.empty()) {
		throw std::runtime_error("Location path cannot be empty");
	}

	if (path[0] != '/') {
		throw std::runtime_error("Location path must start with '/': " + path);
	}
}

// Validate that the HTTP method is one of the allowed ones
void LocationConfig::_validateMethod(const std::string& method) const
{
	const std::string validMethods[] = {"GET", "POST", "DELETE"};
	const int count = sizeof(validMethods)/sizeof(validMethods[0]);

	for (int i = 0; i < count; i++) {
		if (method == validMethods[i]) {
			return;
		}
	}

	throw std::runtime_error("Invalid HTTP method: " + method);
}

// Validate that the CGI extension starts with '.' and contains only valid characters
void LocationConfig::_validateExtension(const std::string& ext) const
{
	if (ext.empty() || ext[0] != '.') {
		throw std::runtime_error("CGI extension must start with '.': " + ext);
	}

	for (size_t i = 1; i < ext.size(); i++) {
		if (!std::isalnum(ext[i]) && ext[i] != '_' && ext[i] != '-') {
			throw std::runtime_error("Invalid character in CGI extension: " + ext);
		}
	}
}

// Validate that the redirect status code is in the correct range
void LocationConfig::_validateStatusCode(int code) const
{
	if (code < 300 || code > 599) {
		std::ostringstream oss;
		oss << "Invalid redirect status code: " << code
			<< " (must be 3xx, 4xx or 5xx)";
		throw std::runtime_error(oss.str());
	}
}

// Set the location path after validation
void LocationConfig::setPath(const std::string& p)
{
	_validatePath(p);
	_path = p;
}

// Set the root path after validation
void LocationConfig::setRoot(const std::string& r)
{
	if (r.empty()) {
		throw std::runtime_error("Root path cannot be empty");
	}
	_root = r;
}

// Add an index file, avoiding duplicates and validating input
void LocationConfig::addIndex(const std::string& idx)
{
	if (idx.empty()) {
		throw std::runtime_error("Index file name cannot be empty");
	}
	// Avoid duplicates
	for (std::vector<std::string>::const_iterator it = _indexes.begin();
		it != _indexes.end(); ++it)
	{
		if (*it == idx) {
			return;
		}
	}
	_indexes.push_back(idx);
}

// Enable or disable autoindex
void LocationConfig::setAutoIndex(bool a)
{
	_autoindex = a;
}

// Add an allowed HTTP method, avoiding duplicates and validating input
void LocationConfig::addAllowedMethod(const std::string& m)
{
	_validateMethod(m);

	// Avoid duplicates
	for (std::vector<std::string>::const_iterator it = _allowed_methods.begin();
		it != _allowed_methods.end(); ++it)
	{
		if (*it == m) {
			return;
		}
	}
	_allowed_methods.push_back(m);
}

// Add a redirect, validating status code and target
void LocationConfig::addRedirect(int code, const std::string& target)
{
	_validateStatusCode(code);

	if (target.empty()) {
		throw std::runtime_error("Redirect target cannot be empty");
	}
	_redirects[code] = target;
}

// Add a CGI handler, validating extension and path
void LocationConfig::addCgi(const std::string& ext, const std::string& cgi_path)
{
	_validateExtension(ext);

	if (cgi_path.empty()) {
		throw std::runtime_error("CGI path cannot be empty");
	}
	_cgis[ext] = cgi_path;
}

// Set the list of index files, validating all entries and avoiding empty names
void LocationConfig::setIndexes(const std::vector<std::string>& indexes)
{
	if (indexes.empty()) {
		throw std::runtime_error("Index list cannot be empty");
	}
	for (std::vector<std::string>::const_iterator it = indexes.begin();
		it != indexes.end(); ++it)
	{
		if (it->empty()) {
			throw std::runtime_error("Index file name in list cannot be empty");
		}
	}
	_indexes = indexes;
}

// Getters
const std::string& LocationConfig::getPath() const { return _path; }
const std::string& LocationConfig::getRoot() const { return _root; }
const std::vector<std::string>& LocationConfig::getIndexes() const { return _indexes; }
bool LocationConfig::isAutoIndex() const { return _autoindex; }
const std::vector<std::string>& LocationConfig::getAllowedMethods() const { return _allowed_methods; }
const std::map<int, std::string>& LocationConfig::getRedirects() const { return _redirects; }
const std::map<std::string, std::string>& LocationConfig::getCgis() const { return _cgis; }

// Inherit missing values from the server config
LocationConfig LocationConfig::inheritFromServer(const ServerConfig& server) const
{
	LocationConfig result = *this;

	// Inherit root if not set
	if (result.getRoot().empty()) {
		result.setRoot(server.getServerRoot());
	}

	// Inherit indexes if not set
	if (result.getIndexes().empty()) {
		result.setIndexes(server.getServerIndexes());
	}

	return result;
}
