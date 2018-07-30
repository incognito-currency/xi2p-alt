CXXFLAGS += -Wall -Wextra -pedantic -O0 -g -std=c++11 -D_GLIBCXX_USE_NANOSLEEP=1 -I../libxi2p/ -pthread -Wl,--unresolved-symbols=ignore-in-object-files

TESTS = test-gost test-gost-sig test-base-64 test-x25519 test-aeadchacha20poly1305

all: $(TESTS) run

test-http-%: ../libxi2p/HTTP.cpp test-http-%.cpp
	$(CXX) $(CXXFLAGS) $(NEEDED_CXXFLAGS) $(INCFLAGS) -o $@ $^

test-base-%: ../libxi2p/Base.cpp test-base-%.cpp
	$(CXX) $(CXXFLAGS) $(NEEDED_CXXFLAGS) $(INCFLAGS) -o $@ $^

test-gost: ../libxi2p/Gost.cpp ../libxi2p/I2PEndian.cpp test-gost.cpp
	$(CXX) $(CXXFLAGS) $(NEEDED_CXXFLAGS) $(INCFLAGS) -o $@ $^ -lcrypto

test-gost-sig: ../libxi2p/Gost.cpp ../libxi2p/I2PEndian.cpp ../libxi2p/Crypto.cpp ../libxi2p/Log.cpp test-gost-sig.cpp
	$(CXX) $(CXXFLAGS) $(NEEDED_CXXFLAGS) $(INCFLAGS) -o $@ $^ -lcrypto -lssl -lboost_system

test-x25519: ../libxi2p/Ed25519.cpp ../libxi2p/I2PEndian.cpp ../libxi2p/Log.cpp ../libxi2p/Crypto.cpp  test-x25519.cpp
	$(CXX) $(CXXFLAGS) $(NEEDED_CXXFLAGS) $(INCFLAGS) -o $@ $^ -lcrypto -lssl -lboost_system

test-aeadchacha20poly1305: ../libxi2p/Crypto.cpp ../libxi2p/ChaCha20.cpp ../libxi2p/Poly1305.cpp test-aeadchacha20poly1305.cpp
	 $(CXX) $(CXXFLAGS) $(NEEDED_CXXFLAGS) $(INCFLAGS) -o $@ $^ -lcrypto -lssl -lboost_system

run: $(TESTS)
	@for TEST in $(TESTS); do ./$$TEST ; done

clean:
	rm -f $(TESTS)
