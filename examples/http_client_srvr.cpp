/** 
* HTTP Client test example.
*/ 


#include <uWS/uWS.h>
#include <uWs/Hub.h>
#include <iostream>

std::mutex m;

int main()
{


	//****
	// HTTP Server Handlers.
	//****
	uWS::Hub hubSrv;

	hubSrv.onHttpConnection([](uWS::HttpSocket<uWS::SERVER> *ws) 
	{
		m.lock();
		std::cout << "SERVER: Http Socket Connecting" << std::endl;
		m.unlock();
	});

	hubSrv.onHttpRequest([](uWS::HttpResponse *res, uWS::HttpRequest req, char *data, size_t length, size_t remainingBytes)
	{
		uWS::Header url = req.getUrl();
		std::string sUrl = url.toString();

		m.lock();
		std::cout << "SERVER: HTTP Request - " << sUrl << std::endl;
		m.unlock();
		uWS::HttpMethod mthd = req.getMethod();
		if (mthd == uWS::HttpMethod::METHOD_GET) {
			std::string sResponse = "The server just sent this message back.";
			//res->write();
			res->end(uWS::STATUS_OK, sResponse.c_str(), sResponse.length());
			return;
		}
		else if (mthd == uWS::HttpMethod::METHOD_PUT)
		{
			std::string sData(data, length);
			m.lock();
			std::cout << "SERVER: Received Data -- " << sData << std::endl;
			m.unlock();
			res->end(uWS::STATUS_OK);
		}

	});

	hubSrv.onError([](void *user) {
		m.lock();
		std::cout << "SERVER: FAILURE - " << user << " should not emit error!" << std::endl;
		m.unlock();
		exit(-1);
	});
	
	if(!hubSrv.listen(3000))
	{
		std::cout << "SERVER: error listening" << std::endl;
		exit(-1);
	}

	//****
	// HTTP Client handlers implemented on separate thread.
	//****
	new std::thread([] 
	{ 
		uWS::Hub hubClient;
		hubClient.onHttpConnection([](uWS::HttpSocket<uWS::CLIENT> *ws)
		{
			m.lock();
			std::cout << "CLIENT: Socket Connected" << std::endl;
			m.unlock();
		});

		hubClient.onHttpResponse([](const uWS::HttpResponseHeader& resp, char* data, size_t len, size_t maxlen)
		{
			std::string sStatus = resp.getStatusString();
			uWS::HttpStatusCode code = resp.getStatusCode();
			std::string sProtocol = resp.getProtocol();
			std::string sval(data, len);
			m.lock();
			std::cout << "CLIENT: " << sval<< std::endl;
			m.unlock();
		});

		hubClient.onHttpDisconnection([](uWS::HttpSocket<false>* psock)
		{
			m.lock();
			std::cout << "CLIENT: disconnected" << std::endl;
			m.unlock();
		});

		hubClient.onError([](void *user) {
			m.lock();
			std::cout << "CLIENT: FAILURE -- " << user << " should not emit error!" << std::endl;
			m.unlock();
			exit(-1);
		});

		//Get default content
		std::string suri = "http://localhost:3000/MyContent";
		hubClient.connect(suri, uWS::METHOD_GET, nullptr);
		
		//Put some content
		std::string sData = "Arrived just now, the CLIENT just PUT this message.";
		hubClient.connect(suri, uWS::METHOD_PUT, nullptr, {}, sData.c_str(), sData.size());


		//Run and let process both connects.
		hubClient.run();

	});

	//Let the server run.
	hubSrv.run();

}
