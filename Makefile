

rtstreamplayer: rtstreamplayer.cpp
	g++ $< -lSDL -lsndfile -o $@ -lpthread -g
 

clean:
	rm rtstreamplayer -f
