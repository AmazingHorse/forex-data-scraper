#include "PosixTestClient.h"

#include "EPosixClientSocket.h"
#include "EPosixClientSocketPlatform.h"

#include "Contract.h"
#include "Order.h"

#include <stdio.h>

const int PING_DEADLINE = 2; // seconds
const int SLEEP_BETWEEN_PINGS = 30; // seconds

///////////////////////////////////////////////////////////
// member funcs
PosixTestClient::PosixTestClient()
	: m_pClient(new EPosixClientSocket(this))
	, m_state(ST_CONNECT)
	, m_sleepDeadline(0)
	, m_orderId(0)
    , ticker_id(0)
{
}

PosixTestClient::~PosixTestClient()
{
}

bool PosixTestClient::connect(const char *host, unsigned int port, int clientId)
{
	// trying to connect
	printf( "Connecting to %s:%d clientId:%d\n", !( host && *host) ? "127.0.0.1" : host, port, clientId);

	bool bRes = m_pClient->eConnect( host, port, clientId, /* extraAuth */ false);

	if (bRes) {
		printf( "Connected to %s:%d clientId:%d\n", !( host && *host) ? "127.0.0.1" : host, port, clientId);
	}
	else
		printf( "Cannot connect to %s:%d clientId:%d\n", !( host && *host) ? "127.0.0.1" : host, port, clientId);

	return bRes;
}

void PosixTestClient::disconnect() const
{
	m_pClient->eDisconnect();

	printf ( "Disconnected\n");
}

bool PosixTestClient::isConnected() const
{
	return m_pClient->isConnected();
}
// Accepts state, pointer to message, and pointer to id
void PosixTestClient::processMessages()
{
    // Initialize the file descriptor sets needed for select()
	fd_set readSet, writeSet, errorSet;

    // Get the current time and initialize a struct for select()
	struct timeval tval;
	tval.tv_usec = 0;
	tval.tv_sec = 0;
    time_t now = time(NULL);

    // Perform the required operation based on the state passed in
	switch (m_state) {
		case ST_PLACEORDER:
			placeOrder();
			break;
		case ST_PLACEORDER_ACK:
			break;
		case ST_CANCELORDER:
			cancelOrder();
			break;
		case ST_CANCELORDER_ACK:
			break;
		case ST_PING:
			reqCurrentTime();
			break;
		case ST_PING_ACK:
			if( m_sleepDeadline < now) {
				disconnect();
				return;
			}
			break;
		case ST_IDLE:
			if( m_sleepDeadline < now) {
				m_state = ST_PING;
				return;
			}
			break;
	}

	if( m_sleepDeadline > 0) {
		// initialize timeout with m_sleepDeadline - now
		tval.tv_sec = m_sleepDeadline - now;
	}
    // If we have a valid socket client file descriptor
	if( m_pClient->fd() >= 0 ) {
        // Initialize the file descriptor sets
		FD_ZERO( &readSet);
		errorSet = writeSet = readSet;
        // Add the client's file descriptor to readfds
		FD_SET( m_pClient->fd(), &readSet);
        // If the output buffer of the socket client is not empty
		if( !m_pClient->isOutBufferEmpty()) {
            // Add the client's fd to writefds as well
			FD_SET( m_pClient->fd(), &writeSet);
        }
        // Add the client's file descriptor to errorfds
		FD_SET( m_pClient->fd(), &errorSet);
        // Monitor the file descriptor for any events
		int ret = select( m_pClient->fd() + 1, &readSet, &writeSet, &errorSet, &tval);

		if( ret == 0) { // timeout
			return;
		}

		if( ret < 0) {	// error
			disconnect();
			return;
		}
        // fd error
		if( m_pClient->fd() < 0)
			return;
        // if the socket's fd is still in the error set
		if( FD_ISSET( m_pClient->fd(), &errorSet)) {
            // error on socket
			m_pClient->onError();
		}
        // if onError is called, eDisconnect sets the fd to -1, so exit
		if( m_pClient->fd() < 0)
			return;
        // if the socket's fd is still in the write set
		if( FD_ISSET( m_pClient->fd(), &writeSet)) {
			// socket is ready for writing
			m_pClient->onSend();
		}
        // if there is an error, eDisconnect sets the fd to -1, so exit
		if( m_pClient->fd() < 0)
			return;
        //if the socket's fd is still in the read set
		if( FD_ISSET( m_pClient->fd(), &readSet)) {
			// socket is ready for reading
			m_pClient->onReceive();
		}
	}
}

//////////////////////////////////////////////////////////////////
// methods
void PosixTestClient::reqCurrentTime()
{
	printf( "Requesting Current Time\n");

	// set ping deadline to "now + n seconds"
	m_sleepDeadline = time( NULL) + PING_DEADLINE;

	m_state = ST_PING_ACK;

	m_pClient->reqCurrentTime();
}

void PosixTestClient::placeOrder()
{
	Contract contract;
	Order order;

	contract.symbol = "IBM";
	contract.secType = "STK";
	contract.exchange = "SMART";
	contract.currency = "USD";

	order.action = "BUY";
	order.totalQuantity = 1000;
	order.orderType = "LMT";
	order.lmtPrice = 0.01;

	printf( "Placing Order %ld: %s %ld %s at %f\n", ++m_orderId, order.action.c_str(), order.totalQuantity, contract.symbol.c_str(), order.lmtPrice);

	m_state = ST_PLACEORDER_ACK;

	m_pClient->placeOrder( m_orderId, contract, order);
}

void PosixTestClient::cancelOrder()
{
	printf( "Cancelling Order %ld\n", m_orderId);

	m_state = ST_CANCELORDER_ACK;

	m_pClient->cancelOrder( m_orderId);
}

///////////////////////////////////////////////////////////////////
// events
void PosixTestClient::orderStatus( OrderId orderId, const std::string& status, int filled,
	   int remaining, double avgFillPrice, int permId, int parentId,
	   double lastFillPrice, int clientId, const std::string& whyHeld)

{
	if( orderId == m_orderId) {
		if( m_state == ST_PLACEORDER_ACK && (status == "PreSubmitted" || status == "Submitted"))
			m_state = ST_CANCELORDER;

		if( m_state == ST_CANCELORDER_ACK && status == "Cancelled")
			m_state = ST_PING;

		printf( "Order: id=%ld, status=%s\n", orderId, status.c_str());
	}
}

void PosixTestClient::historicalData(const std::string& date, 
        double open, double high, double low, double close, int volume, 
        int barCount, double WAP, int hasGaps) {
    // Perform some validation
    // Request historical data from TWS socket
    m_pClient->reqHistoricalData(++ticker_id, date, open, high, low, close, volume, 
            barCount, WAP, hasGaps);    
}

void PosixTestClient::nextValidId( OrderId orderId)
{
	m_orderId = orderId;
}

void PosixTestClient::currentTime( long time)
{
	if ( m_state == ST_PING_ACK) {
		time_t t = ( time_t)time;
		struct tm * timeinfo = localtime ( &t);
		printf( "The current date/time is: %s", asctime( timeinfo));

		time_t now = ::time(NULL);
		m_sleepDeadline = now + SLEEP_BETWEEN_PINGS;

		m_state = ST_IDLE;
	}
}

void PosixTestClient::error(const int id, const int errorCode, const std::string errorString)
{
//	printf( "Error id=%d, errorCode=%d, msg=%s\n", id, errorCode, errorString.c_str());

	if( id == -1 && errorCode == 1100) // if "Connectivity between IB and TWS has been lost"
		disconnect();
}

void PosixTestClient::tickPrice( TickerId tickerId, TickType field, double price, int canAutoExecute) {}
void PosixTestClient::tickSize( TickerId tickerId, TickType field, int size) {}
void PosixTestClient::tickOptionComputation( TickerId tickerId, TickType tickType, double impliedVol, double delta,
											 double optPrice, double pvDividend,
											 double gamma, double vega, double theta, double undPrice) {}
void PosixTestClient::tickGeneric(TickerId tickerId, TickType tickType, double value) {}
void PosixTestClient::tickString(TickerId tickerId, TickType tickType, const std::string& value) {}
void PosixTestClient::tickEFP(TickerId tickerId, TickType tickType, double basisPoints, const std::string& formattedBasisPoints,
							   double totalDividends, int holdDays, const std::string& futureExpiry, double dividendImpact, double dividendsToExpiry) {}
void PosixTestClient::openOrder( OrderId orderId, const Contract&, const Order&, const OrderState& ostate) {}
void PosixTestClient::openOrderEnd() {}
void PosixTestClient::winError( const std::string& str, int lastError) {}
void PosixTestClient::connectionClosed() {}
void PosixTestClient::updateAccountValue(const std::string& key, const std::string& val,
										  const std::string& currency, const std::string& accountName) {}
void PosixTestClient::updatePortfolio(const Contract& contract, int position,
		double marketPrice, double marketValue, double averageCost,
		double unrealizedPNL, double realizedPNL, const std::string& accountName){}
void PosixTestClient::updateAccountTime(const std::string& timeStamp) {}
void PosixTestClient::accountDownloadEnd(const std::string& accountName) {}
void PosixTestClient::contractDetails( int reqId, const ContractDetails& contractDetails) {}
void PosixTestClient::bondContractDetails( int reqId, const ContractDetails& contractDetails) {}
void PosixTestClient::contractDetailsEnd( int reqId) {}
void PosixTestClient::execDetails( int reqId, const Contract& contract, const Execution& execution) {}
void PosixTestClient::execDetailsEnd( int reqId) {}
void PosixTestClient::updateMktDepth(TickerId id, int position, int operation, int side,
									  double price, int size) {}
void PosixTestClient::updateMktDepthL2(TickerId id, int position, std::string marketMaker, int operation,
										int side, double price, int size) {}
void PosixTestClient::updateNewsBulletin(int msgId, int msgType, const std::string& newsMessage, const std::string& originExch) {}
void PosixTestClient::managedAccounts( const std::string& accountsList) {}
void PosixTestClient::receiveFA(faDataType pFaDataType, const std::string& cxml) {}
void PosixTestClient::scannerParameters(const std::string& xml) {}
void PosixTestClient::scannerData(int reqId, int rank, const ContractDetails& contractDetails,
	   const std::string& distance, const std::string& benchmark, const std::string& projection,
	   const std::string& legsStr) {}
void PosixTestClient::scannerDataEnd(int reqId) {}
void PosixTestClient::realtimeBar(TickerId reqId, long time, double open, double high, double low, double close,
								   long volume, double wap, int count) {}
void PosixTestClient::fundamentalData(TickerId reqId, const std::string& data) {}
void PosixTestClient::deltaNeutralValidation(int reqId, const UnderComp& underComp) {}
void PosixTestClient::tickSnapshotEnd(int reqId) {}
void PosixTestClient::marketDataType(TickerId reqId, int marketDataType) {}
void PosixTestClient::commissionReport( const CommissionReport& commissionReport) {}
void PosixTestClient::position( const std::string& account, const Contract& contract, int position, double avgCost) {}
void PosixTestClient::positionEnd() {}
void PosixTestClient::accountSummary( int reqId, const std::string& account, const std::string& tag, const std::string& value, const std::string& curency) {}
void PosixTestClient::accountSummaryEnd( int reqId) {}
void PosixTestClient::verifyMessageAPI( const std::string& apiData) {}
void PosixTestClient::verifyCompleted( bool isSuccessful, const std::string& errorText) {}
void PosixTestClient::displayGroupList( int reqId, const std::string& groups) {}
void PosixTestClient::displayGroupUpdated( int reqId, const std::string& contractInfo) {}
