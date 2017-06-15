

rtstreamplayer: rtstreamplayer.cpp
	$(CXX) $< -lSDL -lsndfile -o $@ -lpthread -g  -std=c++0x
 

clean:
	rm rtstreamplayer -f
