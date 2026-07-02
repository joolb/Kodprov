#include <iostream>
#include <string>
#include <vector>
#include <memory>

#include <asio.hpp>

struct Object
{
	int64_t ID = 0;
	int32_t X = 0;
	int32_t Y = 0;
	uint8_t TYPE = 0;
};

class Middleware
{
public:
	Middleware() : 
		m_Timer(m_IOContext), 
		m_Socket(m_IOContext),
		m_Istream(&m_Buffer)
	{}

	void Connect(const std::string& host, const std::string& port)
	{
		try
		{
			asio::ip::tcp::resolver resolver(m_IOContext);
			// Resolves host and service names into a list of endpoint entries
			auto endpoints = resolver.resolve(host, port);

			// Attempts to connect a socket to one of a sequence of endpoints, until a connection is successfully established
			asio::async_connect(m_Socket, endpoints,
				[this](const std::error_code& e, asio::ip::tcp::endpoint)
				{
					if (!e)
					{
						Read();
						Write();
					}
					else
					{
						std::clog << e.message() << std::endl;
					}
				});

			m_IOContext.run();
		}
		catch (const std::exception& e)
		{
			std::clog << e.what() << std::endl;
		}
	}

private:
	// Parses data from the server and updates the elemenets in m_Objets according to their new state
	void Read()
	{
		// Reads data into specified buffer until buffer's get area contains delimiter character
		asio::async_read_until(m_Socket, m_Buffer, '\n',
			[this](const std::error_code& e, std::size_t)
			{
				if (!e)
				{
					std::string str;
					std::getline(m_Istream, str);

					try
					{
						Object newObjData;

						int32_t keyValPairs = 0;
						size_t pos = 0;
						// Extracts the key-value pair from the current line 
						// Input format: ID<S64>;X=<S32>;Y=<S32>;TYPE=<U8>
						while (pos < str.size())
						{
							size_t start = str.find('=', pos);
							size_t end = str.find(';', pos);

							if (end == std::string::npos)
								end = str.size();

							std::string key = str.substr(pos, start - pos);

							if (key == "ID")
							{
								newObjData.ID = std::stoll(str.substr(start + 1, end - 1));
							}
							else if (key == "X")
							{
								newObjData.X = std::stoi(str.substr(start + 1, end - 1));
							}
							else if (key == "Y")
							{
								newObjData.Y = std::stoi(str.substr(start + 1, end - 1));
							}
							else if (key == "TYPE")
							{
								uint8_t TYPE = std::stoi(str.substr(start + 1, end - 1));
								if (TYPE < 1 || TYPE > 3)
								{
									throw std::invalid_argument("TYPE out of range");
								}
								newObjData.TYPE = TYPE;
							}

							keyValPairs++;
							pos = end + 1;
						}

						if (keyValPairs != 4)
							throw std::invalid_argument("Incomplete input data");

						// Updates the object in m_Objects, or adds a new one if it does not exist
						bool objectExists = false;
						for (Object& object : m_Objects)
						{
							if (object.ID == newObjData.ID)
							{
								objectExists = true;
								object = newObjData;
								break;
							}
						}
						if (!objectExists)
							m_Objects.push_back(newObjData);
					}
					catch (const std::invalid_argument& e)
					{
						std::clog << e.what() << std::endl;
					}
					catch (const std::out_of_range& e) {
						std::clog << e.what() << std::endl;
					}

					Read();
				}
				else
					std::clog << e.message() << std::endl;
			});
	}

	// Periodically writes the current state of the elements in m_Objects to standard output
	void Write()
	{
		m_Timer.expires_after(asio::chrono::milliseconds(5000 / 3));
		m_Timer.async_wait(
			[this](const std::error_code& e)
			{
				if (!e)
				{
					uint32_t preamble = 0xFE00;
					uint32_t count = m_Objects.size();

					std::cout.write(reinterpret_cast<const char*>(&preamble), sizeof(uint32_t));
					std::cout.write(reinterpret_cast<const char*>(&count), sizeof(uint32_t));

					for (Object& object : m_Objects)
					{
						float distance = sqrt(pow(150 - object.X, 2) + pow(150 - object.Y, 2));
						uint8_t color[3] = {0x5B, 0x34, 0x6D};
						if ((object.TYPE == 1 && distance < 50) || (object.TYPE == 3 && distance < 100))
						{
							color[1] = 0x31;
						}
						else if ((object.TYPE == 1 && distance < 75) || (object.TYPE == 2 && distance < 50) || (object.TYPE == 3))
						{
							color[1] = 0x33;
						}

						std::cout.write(reinterpret_cast<const char*>(&object.ID), sizeof(int64_t));
						std::cout.write(reinterpret_cast<const char*>(&object.X), sizeof(int32_t));
						std::cout.write(reinterpret_cast<const char*>(&object.Y), sizeof(int32_t));
						std::cout.write(reinterpret_cast<const char*>(&object.TYPE), sizeof(uint8_t));
						std::cout.write(reinterpret_cast<const char*>(&color), sizeof(uint8_t) * 3);
					}

					Write();
				}
				else
					std::clog << e.message() << std::endl;
			});
	}

private:
	asio::io_context m_IOContext;
	asio::steady_timer m_Timer;
	asio::ip::tcp::socket m_Socket;
	asio::streambuf m_Buffer;
	std::vector<Object> m_Objects;
	std::istream m_Istream;
};

int main()
{
	auto middleware = std::make_shared<Middleware>();
	middleware->Connect("localhost", "5463");
}