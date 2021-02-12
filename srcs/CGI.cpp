#include "CGI.hpp"

CGI::CGI(const std::string& path, const std::string& source, const Request& request)
	: _cgiPath(path), _cgiSource(source), _request(request)
{ }

CGI::~CGI()
{ }

CGI::CGI(const CGI& cgi)
	: _cgiPath(cgi._cgiPath), _cgiSource(cgi._cgiSource), _request(cgi._request)
{ }

std::string CGI::processCGI(const Host& host)
{
	std::string result;

	try {
		result = executeCGI(host);
	}
	catch (const std::exception& ex) {
		std::cerr << "CGI exception: " << ex.what() << std::endl;
	}
	return (result);
}

std::string CGI::executeCGI(const Host& host)
{
	int		fd[2];
	pid_t	pid;
	int 	exec_status = 0;
	int		status = 0;
	char** args = formArgs();
	char** envs = formEnvs(host);

	if (pipe(fd) < 0)
		throw std::runtime_error("pipe fails");
	pid = fork();
	if (pid < 0) {
		throw std::runtime_error("fork fails");
	}
	else if (pid > 0) {
		close(fd[0]);
		write(fd[1], this->_request.getContent().c_str(), this->_request.getContent().length());
		close(fd[1]);
		waitpid(pid, &status, 0);
		if (WIFEXITED(status))
			status = WEXITSTATUS(status);
		if (status)
			throw std::runtime_error("script execution failed");
	}
	else {
		close(fd[1]);
		int outputFd = open("./cgi_response", O_RDWR | O_CREAT | O_TRUNC,
			S_IRWXU | S_IRGRP | S_IROTH);
		if (outputFd < 0)
			throw std::runtime_error("open fails");
		if (dup2(fd[0], 0) < 0)
			throw std::runtime_error("dup2 fails");
		if (dup2(outputFd, 1) < 0)
			throw std::runtime_error("dup2 fails");
		exec_status = execve(this->_cgiPath.c_str(), args, envs);
		close(outputFd);
		close(fd[0]);
		exit(exec_status);
	}
	freeMatrix(args);
	freeMatrix(envs);
	return (getFileContent("./cgi_response"));
}

char** CGI::formArgs() const
{
	char** args = new char*[3];
	args[0] = stringDup(this->_cgiPath);
	args[1] = stringDup(this->_cgiSource);
	args[2] = NULL;
	return (args);
}

char** CGI::formEnvs(const Host& host) const
{
	std::map<std::string, std::string> strEnvs;

	if (this->_request.getHeaders().count("authorization")) {
		std::vector<std::string> authVec = split(this->_request.getHeaders()
			.at("authorization"), " ");
		strEnvs["AUTH_TYPE"] = authVec[0];
		if (authVec[0] == "Basic") {
			std::vector<std::string> splitBase64 = split(decodeBase64(authVec[1]), ":");
			strEnvs["REMOTE_USER"] = splitBase64[0];
			strEnvs["REMOTE_IDENT"] = splitBase64[1];
		}
		else {
			strEnvs["REMOTE_USER"] = "";
			strEnvs["REMOTE_IDENT"] = "";
		}
	}
	strEnvs["REMOTE_ADDR"] = iptoa(this->_request.getSockAddr().sin_addr.s_addr);
	strEnvs["CONTENT_LENGTH"] = this->_request.getContent().length() != 0 ?
		std::to_string(this->_request.getContent().length()) : "";
	strEnvs["CONTENT_TYPE"] = this->_request.getHeaders().count("content-type") ?
		this->_request.getHeaders().at("content-type") : "";
	strEnvs["GATEWAY_INTERFACE"] = "CGI/1.1";
	strEnvs["SERVER_PROTOCOL"] = "HTTP/1.1";
	strEnvs["SERVER_SOFTWARE"] = "webserv/1.0";
	strEnvs["SERVER_NAME"] = host.getName();
	strEnvs["SERVER_PORT"] = std::to_string(host.getPort());
	strEnvs["REQUEST_METHOD"] = this->_request.getMethod();

	char pwd[1024];
	getcwd(pwd, 1024);
	std::string requestUri, pathInfo, queryStr, requestPath;
	if (this->_request.getPath().find_first_of('?') != std::string::npos) {
		requestPath = this->_request.getPath().substr(0, this->_request.getPath().find_first_of('?'));
		queryStr = this->_request.getPath().substr(this->_request.getPath().find_first_of('?') + 1);
	}
	else {
		requestPath = this->_request.getPath();
		queryStr = "";
	}
	std::vector<std::string> splitPath = split(requestPath, "/");

	std::vector<std::string>::iterator it;
	std::string path(pwd);
	struct stat stat_buff;
	for (it = splitPath.begin(); it != splitPath.end(); it++) {
		path += "/" + *it;
		requestUri += "/" + *it;
		stat(path.c_str(), &stat_buff);
		if (S_ISREG(stat_buff.st_mode) == true) {
			break ;
		}
	}

	/*
	// rfc logic
	if (it == splitPath.end()) {
		throw std::runtime_error("404"); // not found ???
	}
	else if (it == --splitPath.end()) {
		pathInfo = requestUri;
	}
	else {
		for (std::vector<std::string>::iterator jt = it + 1; jt != splitPath.end(); jt++)
			pathInfo += "/" + *jt;
	}
	*/

	// for tester
	pathInfo = requestUri;

	strEnvs["REQUEST_URI"] = requestUri;
	strEnvs["QUERY_STRING"] = queryStr;
	strEnvs["SCRIPT_NAME"] = requestUri.substr(requestUri.find_last_of('/'));
	strEnvs["PATH_INFO"] = pathInfo;
	strEnvs["PATH_TRANSLATED"] = pwd + pathInfo;

	std::map<std::string, std::string> requestHeaders = this->_request.getHeaders();
	for (std::map<std::string, std::string>::iterator jt = requestHeaders.begin(); jt != requestHeaders.end(); jt++) {
		std::string header = jt->first;
		if (header.find("-") != std::string::npos)
			header.replace(header.find("-"), 1, "_");
		std::transform(header.begin(), header.end(), header.begin(), toupper);
		strEnvs["HTTP_" + header] = jt->second;
	}

	char** envs = new char* [strEnvs.size() + 1];
	size_t i = 0;
	for (std::map<std::string, std::string>::iterator jt = strEnvs.begin(); jt != strEnvs.end(); jt++)
		envs[i++] = stringDup(jt->first + "=" + jt->second);
	envs[i] = NULL;

	return (envs);
}

std::string CGI::decodeBase64(const std::string& input) const
{
	char base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string result;
	std::vector<int> base(256,-1);
	for (int i = 0; i < 64; i++)
		base[base64[i]] = i;

	int val = 0, valb = -8;
	for (size_t i = 0; i < input.size(); i++) {
		if (base[input[i]] == -1)
			break;
		val = (val << 6) + base[input[i]];
		valb += 6;
		if (valb >= 0) {
			result.push_back(char((val >> valb) & 0xff));
			valb -= 8;
		}
	}
	return result;
}

const std::string& CGI::getPath() const
{ return (this->_cgiPath); }

const std::string& CGI::getSource() const
{ return (this->_cgiSource); }
