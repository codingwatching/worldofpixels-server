UWS = uWebSockets/src/Extensions.cpp uWebSockets/src/Group.cpp uWebSockets/src/WebSocketImpl.cpp uWebSockets/src/Networking.cpp uWebSockets/src/Hub.cpp uWebSockets/src/Node.cpp uWebSockets/src/WebSocket.cpp uWebSockets/src/HTTPSocket.cpp uWebSockets/src/Socket.cpp

INCLUDE = -I uWebSockets/src/
LIBS = -luv -lcrypto -lssl -lz -lpthread -lcurl -DUSE_LIBUV

OBJS = commands.cpp color.cpp server.cpp database.cpp client.cpp world.cpp limiter.cpp main.cpp AsyncHTTPGETClient.cpp TaskBuffer.cpp

OUT = out

all:
	g++ -std=gnu++0x -Wall -O2 $(INCLUDE) $(UWS) $(OBJS) $(LIBS) -o $(OUT)

debug:
	g++ -std=gnu++0x -Wall -Og -g $(INCLUDE) $(UWS) $(OBJS) $(LIBS) -o $(OUT)
