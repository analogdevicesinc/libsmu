#include "libsmu.hpp"

using std::unique_ptr;

int main(){
    unique_ptr<Session> x = unique_ptr<Session>(new Session());
}
