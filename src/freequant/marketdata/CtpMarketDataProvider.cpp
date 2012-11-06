#include <cstring>
#include <iostream>
#include <vector>

#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/date_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/format.hpp>

#include <freequant/marketdata/Bar.h>

#include "CtpMarketDataProvider.h"
#include "api/trade/win/public/ThostFtdcMdApi.h"

using namespace std;

namespace FreeQuant {

static int requestId = 0;

class CtpMarketDataProvider::Impl : private CThostFtdcMdSpi {
public:
    Impl(FreeQuant::MarketDataProvider::Callback *callback = 0) :
        _callback(callback), _api(0), _connected(false) {}

    virtual ~Impl() {
        disconnect();
    }

    void setCallback(FreeQuant::MarketDataProvider::Callback *callback) {
        _callback = callback;
    }

    virtual void connect(bool block = true) {
        string connection = "protocal=tcp;ip=asp-sim2-front1.financial-trading-platform.com;port=26213;userid=888888;password=888888;brokerid=4070";
        if (_api == 0) {
            _front = "tcp://asp-sim2-front1.financial-trading-platform.com:26213";
            _userId = "888888";
            _password = "888888";;
            _brokerId = "4070";

            _api = CThostFtdcMdApi::CreateFtdcMdApi("");
            _api->RegisterSpi(this);
            _api->RegisterFront(const_cast<char *>(_front.c_str()));
            _api->Init();
        }
        boost::unique_lock<boost::mutex> l(_mutex);
        _condition.wait(l);
    }

    virtual void disconnect(bool block = true) {
        if (_api != 0) {
            _api->RegisterSpi(0);
            _api->Release();
            _api = 0;
        }
    }


    virtual bool isConnected() const {
        return _connected;
    }

    virtual void subscribe(std::vector<std::string> symbols) {
        vector<const char *> items(symbols.size());
        transform(symbols.begin(), symbols.end(), items.begin(), mem_fun_ref(&string::c_str));
        _api->SubscribeMarketData(const_cast<char**>(&items[0]), items.size());
    }

    virtual void unsubscribe(std::vector<std::string> symbols) {
        vector<const char *> items(symbols.size());
        transform(symbols.begin(), symbols.end(), items.begin(), mem_fun_ref(&string::c_str));
        _api->UnSubscribeMarketData(const_cast<char**>(&items[0]), items.size());
    }

    FreeQuant::MarketDataProvider::Callback *_callback;

    boost::mutex _mutex;
    boost::condition_variable _condition;
    std::string _front;
    std::string _userId;
    std::string _password;
    std::string _brokerId;

    CThostFtdcMdApi *_api;
    bool _connected;

    void OnFrontConnected() {
        cerr << "--->>> " << __FUNCTION__ <<  endl;

        CThostFtdcReqUserLoginField req = {};
        _brokerId.copy(req.BrokerID, _brokerId.size());
        _userId.copy(req.UserID, _userId.size());
        _password.copy(req.Password, _password.size());
        int ret = _api->ReqUserLogin(&req, ++requestId);
        cerr << "--->>>>>> call ReqUserLogin " << ((ret==0) ? "success" : "failed") << endl;
    }

    void OnFrontDisconnected(int nReason) {

    }

    void OnHeartBeatWarning(int timeLapse) {
        cerr << "OnHeartBeatWarning..." << timeLapse << endl;
    }

    void OnRspUserLogin(CThostFtdcRspUserLoginField *rspUserLogin, CThostFtdcRspInfoField *rspInfo, int requestID, bool last) {
        cerr << "--->>> " << __FUNCTION__ << endl;
        if (!errorOccurred(rspInfo) && last) {
            cerr << "--->>> TradingDay " << _api->GetTradingDay() << endl;
            _connected = true;
            if (_callback) _callback->onConnected();

            boost::unique_lock<boost::mutex> lock(_mutex);
            _condition.notify_one();
         }
    }

    void OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
        cerr << "--->>> " << __FUNCTION__ << endl;
    }

    void OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
        cout << __FUNCTION__ << endl;
    }

    void OnRspSubMarketData(CThostFtdcSpecificInstrumentField *specificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
            cerr << __FUNCTION__ << endl;
            cerr << "subscribe " <<  specificInstrument->InstrumentID << endl;
    }

    void OnRspUnSubMarketData(CThostFtdcSpecificInstrumentField *specificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {
        cerr << __FUNCTION__ << endl;
        cerr << "unsubscribe " << specificInstrument->InstrumentID << endl;
    }

    void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *depthMarketData) {
        cerr << __FUNCTION__ << endl;
        boost::gregorian::date d = boost::gregorian::from_undelimited_string(depthMarketData->TradingDay);
        string sdate = to_iso_extended_string(d);
        string str = boost::str(boost::format("%s %s.%s") % sdate %
            depthMarketData->UpdateTime % depthMarketData->UpdateMillisec);
        boost::posix_time::ptime dt = boost::posix_time::time_from_string(str);
        Bar bar(depthMarketData->OpenPrice, depthMarketData->HighestPrice, depthMarketData->LowestPrice,
            depthMarketData->ClosePrice, depthMarketData->Volume);
        if (_callback) _callback->onBar(bar);

//         Bar bar(depthMarketData->LastPrice, depthMarketData->HighestPrice, depthMarketData->LowestPrice, depthMarketData->LastPrice);
//         _onBar(bar);

//        TThostFtdcDateType	TradingDay;
//        ///��Լ����
//        TThostFtdcInstrumentIDType	InstrumentID;
//        ///����������
//        TThostFtdcExchangeIDType	ExchangeID;
//        ///��Լ�ڽ������Ĵ���
//        TThostFtdcExchangeInstIDType	ExchangeInstID;
//        ///���¼�
//        TThostFtdcPriceType	LastPrice;
//        ///�ϴν����
//        TThostFtdcPriceType	PreSettlementPrice;
//        ///������
//        TThostFtdcPriceType	PreClosePrice;
//        ///��ֲ���
//        TThostFtdcLargeVolumeType	PreOpenInterest;
//        ///����
//        TThostFtdcPriceType	OpenPrice;
//        ///��߼�
//        TThostFtdcPriceType	HighestPrice;
//        ///��ͼ�
//        TThostFtdcPriceType	LowestPrice;
//        ///����
//        TThostFtdcVolumeType	Volume;
//        ///�ɽ����
//        TThostFtdcMoneyType	Turnover;
//        ///�ֲ���
//        TThostFtdcLargeVolumeType	OpenInterest;
//        ///������
//        TThostFtdcPriceType	ClosePrice;
//        ///���ν����
//        TThostFtdcPriceType	SettlementPrice;
//        ///��ͣ���
//        TThostFtdcPriceType	UpperLimitPrice;
//        ///��ͣ���
//        TThostFtdcPriceType	LowerLimitPrice;
//        ///����ʵ��
//        TThostFtdcRatioType	PreDelta;
//        ///����ʵ��
//        TThostFtdcRatioType	CurrDelta;
//        ///����޸�ʱ��
//        TThostFtdcTimeType	UpdateTime;
//        ///����޸ĺ���
//        TThostFtdcMillisecType	UpdateMillisec;
//        ///�����һ
//        TThostFtdcPriceType	BidPrice1;
//        ///������һ
//        TThostFtdcVolumeType	BidVolume1;
//        ///������һ
//        TThostFtdcPriceType	AskPrice1;
//        ///������һ
//        TThostFtdcVolumeType	AskVolume1;
//        ///����۶�
//        TThostFtdcPriceType	BidPrice2;
//        ///��������
//        TThostFtdcVolumeType	BidVolume2;
//        ///�����۶�
//        TThostFtdcPriceType	AskPrice2;
//        ///��������
//        TThostFtdcVolumeType	AskVolume2;
//        ///�������
//        TThostFtdcPriceType	BidPrice3;
//        ///��������
//        TThostFtdcVolumeType	BidVolume3;
//        ///��������
//        TThostFtdcPriceType	AskPrice3;
//        ///��������
//        TThostFtdcVolumeType	AskVolume3;
//        ///�������
//        TThostFtdcPriceType	BidPrice4;
//        ///��������
//        TThostFtdcVolumeType	BidVolume4;
//        ///��������
//        TThostFtdcPriceType	AskPrice4;
//        ///��������
//        TThostFtdcVolumeType	AskVolume4;
//        ///�������
//        TThostFtdcPriceType	BidPrice5;
//        ///��������
//        TThostFtdcVolumeType	BidVolume5;
//        ///��������
//        TThostFtdcPriceType	AskPrice5;
//        ///��������
//        TThostFtdcVolumeType	AskVolume5;
//        ///���վ���
//        TThostFtdcPriceType	AveragePrice;
    }

    bool errorOccurred(CThostFtdcRspInfoField *rspInfo) {
        bool occurred = rspInfo && rspInfo->ErrorID != 0;
        if (occurred) {
            cerr << "--->>> ErrorID=" << rspInfo->ErrorID << ", ErrorMsg=" << rspInfo->ErrorMsg << endl;
        }
        return (rspInfo && rspInfo->ErrorID != 0);
    }
};

CtpMarketDataProvider::CtpMarketDataProvider(FreeQuant::MarketDataProvider::Callback *callback) :
    _impl(new CtpMarketDataProvider::Impl(callback)) {
}

CtpMarketDataProvider::~CtpMarketDataProvider() {
    delete _impl;
    _impl = 0;
}

void CtpMarketDataProvider::setCallback(FreeQuant::MarketDataProvider::Callback *callback) {
    _impl->setCallback(callback);
}

void CtpMarketDataProvider::connect(bool block) {
    _impl->connect(block);
}

void CtpMarketDataProvider::disconnect(bool block) {
    _impl->disconnect(block);
}

bool CtpMarketDataProvider::isConnected() const {
    return _impl->isConnected();
}

void CtpMarketDataProvider::subscribe(const Symbols& symbols) {
    _impl->subscribe(symbols);
}

void CtpMarketDataProvider::unsubscribe(const Symbols& symbols) {
    _impl->unsubscribe(symbols);
}

} // namespace FreeQuant

