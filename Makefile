

rtstreamplayer: rtstreamplayer.cpp
	g++ $< -lSDL -lsndfile -o $@


clean:
	rm rtstreamplayer -f
