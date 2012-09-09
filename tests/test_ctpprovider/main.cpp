#include <iostream>

#include <fq/marketdata/provider.h>
#include <fq/marketdata/ctp/ctpprovider.h>

using namespace FreeQuant::MarketData;

int main(int, char* []) {
    Provider *p =  new CtpProvider();
    p->connect();
//    p->disconnect();
    delete p;

    return 0;
}
