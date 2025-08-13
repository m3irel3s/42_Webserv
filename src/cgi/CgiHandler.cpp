/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CgiHandler.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: meferraz <meferraz@student.42porto.pt>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/16 11:33:32 by meferraz          #+#    #+#             */
/*   Updated: 2025/08/13 11:13:40 by meferraz         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "CgiHandler.hpp"
#include "../http/HttpStatus.hpp"

CgiHandler::CgiHandler(const Request &request,
					const ServerConfig &config,
					const LocationConfig &location)
	: _request(request), _config(config), _location(location)
{
	Logger::info("CgiHandler created");
}

CgiHandler::~CgiHandler() {}

Response CgiHandler::execute()
{
	Response response;
	std::string scriptPath = _resolveScriptPath();
	Logger::info("Executing CGI script: " + scriptPath);

	if (!_validateScript(scriptPath)) {
		HttpStatus::buildResponse(_config, response, 404);
		return response;
	}

	_initEnv();

	// Execute CGI synchronously - blocks until complete
	std::string output = _executeCgiSync(scriptPath);

	if (output.empty()) {
		HttpStatus::buildResponse(_config, response, 500);
		return response;
	}

	_parseCgiOutput(output, response);
	return response;
}

bool CgiHandler::_validateScript(const std::string& scriptPath)
{
	if (access(scriptPath.c_str(), F_OK) != 0) {
		return false;
	}

	struct stat stat_buf;
	if (stat(scriptPath.c_str(), &stat_buf) != 0 || !S_ISREG(stat_buf.st_mode)) {
		return false;
	}

	if (access(scriptPath.c_str(), R_OK | X_OK) != 0) {
		return false;
	}

	return true;
}

std::string CgiHandler::_resolveScriptPath() const
{
	std::string base = _location.getRoot();
	if (base.empty()) {
		base = _config.getServerRoot();
	}

	std::string path = _request.getReqPath();
	std::string locPath = _location.getPath();

	// Remove location prefix from path
	if (path.find(locPath) == 0) {
		path = path.substr(locPath.length());
	}

	// Handle trailing slashes
	std::string relPath;
	if (base[base.length() - 1] == '/') {
		relPath = (path[0] == '/') ? base + path.substr(1) : base + path;
	} else {
		relPath = (path[0] == '/') ? base + path : base + "/" + path;
	}

	char absPath[PATH_MAX];
	if (realpath(relPath.c_str(), absPath) != NULL) {
		return std::string(absPath);
	}
	return relPath; // fallback
}

void CgiHandler::_initEnv()
{
	_env["GATEWAY_INTERFACE"] = "CGI/1.1";
	_env["SERVER_SOFTWARE"] = "webserv/1.0";
	_env["QUERY_STRING"] = _request.getReqQueryString();
	_env["REQUEST_METHOD"] = _request.getReqMethod();
	_env["SCRIPT_NAME"] = _request.getReqPath();
	_env["SCRIPT_FILENAME"] = _resolveScriptPath();
	_env["SERVER_PROTOCOL"] = "HTTP/1.1";
	_env["SERVER_NAME"] = _config.getServerHost();
	_env["SERVER_PORT"] = _intToString(_config.getServerPort());
	_env["PATH_INFO"] = _request.getReqPath();
	_env["PATH_TRANSLATED"] = _resolveScriptPath();
	_env["REQUEST_URI"] = _request.getReqPath();
	_env["DOCUMENT_ROOT"] = _location.getRoot().empty() ?
							_config.getServerRoot() : _location.getRoot();

	std::ostringstream contentLength;
	contentLength << _request.getReqBody().size();
	_env["CONTENT_LENGTH"] = contentLength.str();

	std::string contentType = _request.getReqHeaderKey("Content-Type");
	if (!contentType.empty()) {
		_env["CONTENT_TYPE"] = contentType;
	}
	_env["REDIRECT_STATUS"] = "200";

	// Add all headers as HTTP_* variables
	const std::map<std::string, std::string>& headers = _request.getReqHeaders();
	for (std::map<std::string, std::string>::const_iterator it = headers.begin();
		it != headers.end(); ++it)
	{
		std::string env_var = "HTTP_" + it->first;
		std::replace(env_var.begin(), env_var.end(), '-', '_');
		std::transform(env_var.begin(), env_var.end(), env_var.begin(), ::toupper);
		_env[env_var] = it->second;
	}
}

// Create environment array for execve
char** CgiHandler::_createEnvArray() const
{
	char** env = new char*[_env.size() + 1];
	int i = 0;
	for (std::map<std::string, std::string>::const_iterator it = _env.begin();
		it != _env.end(); ++it)
	{
		std::string envVar = it->first + "=" + it->second;
		env[i] = new char[envVar.size() + 1];
		std::strncpy(env[i], envVar.c_str(), envVar.size());
		env[i][envVar.size()] = '\0';
		i++;
	}
	env[i] = NULL;
	return env;
}

// Free environment array
void CgiHandler::_cleanupEnvArray(char** env) const
{
	for (int i = 0; env[i]; i++) {
		delete[] env[i];
	}
	delete[] env;
}

void CgiHandler::_changeToScriptDirectory(const std::string& scriptPath)
{
	char* absolutePath = realpath(scriptPath.c_str(), NULL);
	if (!absolutePath) {
		_exit(EXIT_FAILURE);
	}

	std::string absScriptPath(absolutePath);
	free(absolutePath);

	size_t lastSlash = absScriptPath.find_last_of('/');
	if (lastSlash != std::string::npos) {
		std::string scriptDir = absScriptPath.substr(0, lastSlash);
		if (chdir(scriptDir.c_str()) != 0) {
			_exit(EXIT_FAILURE);
		}
	}
}

void CgiHandler::_execScript(const std::string& scriptPath, char** env)
{
	char* absolutePath = realpath(scriptPath.c_str(), NULL);
	if (!absolutePath) {
		std::cerr << "realpath failed for script: " << scriptPath << std::endl;
		_exit(EXIT_FAILURE);
	}

	std::string absScriptPath(absolutePath);
	free(absolutePath);

	// Extract extension
	size_t dotPos = absScriptPath.find_last_of('.');
	if (dotPos == std::string::npos) {
		std::cerr << "No extension found in script: " << absScriptPath << std::endl;
		_exit(EXIT_FAILURE);
	}
	std::string extension = absScriptPath.substr(dotPos);

	// Lookup interpreter for this extension
	const std::map<std::string, std::string>& cgis = _location.getCgis();
	std::map<std::string, std::string>::const_iterator it = cgis.find(extension);
	if (it == cgis.end()) {
		std::cerr << "No CGI interpreter for extension: " << extension << std::endl;
		_exit(EXIT_FAILURE);
	}
	std::string interpreter = it->second;

	// Prepare arguments: interpreter and script path
	char* argv[3] = {
		const_cast<char*>(interpreter.c_str()),
		const_cast<char*>(absScriptPath.c_str()),
		NULL
	};

	std::cout << "Executing CGI script: " << absScriptPath << std::endl;

	execve(interpreter.c_str(), argv, env);
	std::cerr << "execve failed for: " << interpreter << std::endl;
	_exit(EXIT_FAILURE); // Should not reach here
}

std::string CgiHandler::_executeCgiSync(const std::string& scriptPath)
{
	int pipeOut[2];
	int pipeIn[2];
	if (pipe(pipeOut) < 0) {
		return "";
	}
	if (pipe(pipeIn) < 0) {
		close(pipeOut[0]);
		close(pipeOut[1]);
		return "";
	}

	pid_t pid = fork();
	if (pid < 0) {
		close(pipeOut[0]);
		close(pipeOut[1]);
		close(pipeIn[0]);
		close(pipeIn[1]);
		return "";
	}

	if (pid == 0) {
		// Child process
		close(pipeOut[0]); // Close read end for stdout
		dup2(pipeOut[1], STDOUT_FILENO);
		dup2(pipeOut[1], STDERR_FILENO);
		close(pipeOut[1]);

		close(pipeIn[1]); // Close write end for stdin
		dup2(pipeIn[0], STDIN_FILENO);
		close(pipeIn[0]);

		_changeToScriptDirectory(scriptPath);

		char** env = _createEnvArray();

		_execScript(scriptPath, env);
		_exit(EXIT_FAILURE);
	}

	// Parent process
	close(pipeOut[1]); // Close write end for stdout
	close(pipeIn[0]);  // Close read end for stdin

	// Write POST data to CGI's stdin
	if (!_request.getReqBody().empty()) {
		const std::string& body = _request.getReqBody();
		ssize_t written = 0;
		while (written < (ssize_t)body.size()) {
			ssize_t n = write(pipeIn[1], body.data() + written, body.size() - written);
			if (n <= 0) break;
			written += n;
		}
	}
	close(pipeIn[1]); // Close write end for stdin

	int status;
	waitpid(pid, &status, 0);

	if (WIFEXITED(status)) {
		int exitStatus = WEXITSTATUS(status);
		if (exitStatus != 0) {
			Logger::error("CGI script exited with status: " + _intToString(exitStatus));
		}
	} else if (WIFSIGNALED(status)) {
		Logger::error("CGI script killed by signal: " + _intToString(WTERMSIG(status)));
	}

	std::string output;
	char buffer[4096];
	ssize_t bytes;
	while ((bytes = read(pipeOut[0], buffer, sizeof(buffer))) > 0) {
		output.append(buffer, bytes);
	}

	close(pipeOut[0]);
	return output;
}

static std::string trim(const std::string& str)
{
	size_t first = str.find_first_not_of(" \t\r\n");
	if (first == std::string::npos) {
		return "";
	}
	size_t last = str.find_last_not_of(" \t\r\n");
	return str.substr(first, (last - first + 1));
}

void CgiHandler::_parseCgiOutput(const std::string& output, Response& response)
{
	size_t headerEnd = output.find("\r\n\r\n");
	bool doubleNewline = false;
	if (headerEnd == std::string::npos) {
		headerEnd = output.find("\n\n");
		doubleNewline = true;
		if (headerEnd == std::string::npos) {
			response.setBody(output);
			response.setStatus(200, "OK");
			return;
		}
	}

	std::string headers = output.substr(0, headerEnd);
	std::string body = output.substr(headerEnd + (doubleNewline ? 2 : 4));
	response.setBody(body);

	std::istringstream headerStream(headers);
	std::string headerLine;
	while (std::getline(headerStream, headerLine)) {
		if (headerLine.empty()) continue;
		if (headerLine[headerLine.size()-1] == '\r') {
			headerLine.erase(headerLine.size()-1);
		}

		if (headerLine.find("Status:") == 0) {
			std::string status = headerLine.substr(7);
			status = trim(status);
			size_t spacePos = status.find(' ');
			if (spacePos != std::string::npos) {
				std::string code = status.substr(0, spacePos);
				std::string message = status.substr(spacePos + 1);
				response.setStatus(atoi(code.c_str()), message);
			} else {
				response.setStatus(atoi(status.c_str()), "OK");
			}
		}
		else {
			size_t colonPos = headerLine.find(':');
			if (colonPos != std::string::npos) {
				std::string key = headerLine.substr(0, colonPos);
				std::string value = headerLine.substr(colonPos + 1);
				value = trim(value);
				response.setHeader(key, value);
			}
		}
	}

	if (response.getStatusCode() == 0) {
		response.setStatus(200, "OK");
	}
}

std::string CgiHandler::_intToString(int value) const
{
	std::ostringstream oss;
	oss << value;
	return oss.str();
}
