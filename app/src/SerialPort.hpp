#pragma once

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <optional>
#include <stdexcept>
#include <string>

class SerialPort {
public:
	SerialPort(const std::string& device, speed_t baud = B115200) {
		fd = open(device.c_str(), O_RDWR | O_NOCTTY);

		if (fd < 0) {
			throw std::runtime_error("Failed to open serial port");
		}

		termios tty{};
		if (tcgetattr(fd, &tty) != 0) {
			throw std::runtime_error("Failed to get terminal attributes");
		}

		cfsetispeed(&tty, baud);
		cfsetospeed(&tty, baud);

		tty.c_cflag |= CLOCAL | CREAD;
		tty.c_cflag &= ~CSIZE;
		tty.c_cflag |= CS8;
		tty.c_cflag &= ~PARENB;
		tty.c_cflag &= ~CSTOPB;
		tty.c_cflag &= ~CRTSCTS;

		tty.c_lflag = 0;
		tty.c_iflag = 0;
		tty.c_oflag = 0;

		tty.c_cc[VMIN] = 0;
		tty.c_cc[VTIME] = 1; // 100 ms timeout

		if (tcsetattr(fd, TCSANOW, &tty) != 0) {
			throw std::runtime_error("Failed to configure serial port");
		}

		tcflush(fd, TCIFLUSH);
	}

	~SerialPort() {
		if (fd >= 0) {
			close(fd);
		}
	}

	std::optional<std::string> readLine() {
		char temp[128];

		ssize_t n = read(fd, temp, sizeof(temp));
		if (n > 0) {
			buffer.append(temp, n);
		}

		auto pos = buffer.find('\n');
		if (pos == std::string::npos) {
			return std::nullopt;
		}

		std::string line = buffer.substr(0, pos);

		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}

		buffer.erase(0, pos + 1);

		return line;
	}

private:
	int fd{-1};
	std::string buffer;
};
