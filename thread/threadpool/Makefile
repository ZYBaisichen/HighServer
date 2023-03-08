CC = g++
INCLUDE = -I./
TARGET = web
objs = main.o http_conn.o threadpool.o
$(TARGET):$(objs)
	$(CC) -g $(objs) -o $(TARGET) -pthread
%.o:%.cpp
	$(CC) -g $(INCLUDE) -c $^ -o $@ -pthread