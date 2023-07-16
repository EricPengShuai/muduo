test_boost :
	g++ -o test_boost test_boost.cpp

test_muduo :
	g++ -o test_muduo test_muduo.cpp -lmuduo_net -lmuduo_base -lpthread -std=c++11

test_mymuduo :
	g++ -o test_mymuduo test_mymuduo.cpp -lmymuduo -lpthread

test_mymuduo_g :
	g++ -o test_mymuduo test_mymuduo.cpp -lmymuduo -lpthread -g

clean :
	rm -f test_boost test_muduo test_mymuduo 
