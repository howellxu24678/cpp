/****************************************************************************
** Copyright (c) 2001-2014
**
** This file is part of the QuickFIX FIX Engine
**
** This file may be distributed under the terms of the quickfixengine.org
** license as defined by quickfixengine.org and appearing in the file
** LICENSE included in the packaging of this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See http://www.quickfixengine.org/LICENSE for licensing information.
**
** Contact ask@quickfixengine.org if any conditions of this licensing are
** not clear to you.
**
****************************************************************************/

#ifdef _MSC_VER
#pragma warning( disable : 4503 4355 4786 )
#else
#include "config.h"
#endif

#include "Application.h"
#include "quickfix/Session.h"

#include "quickfix/fix40/ExecutionReport.h"
#include "quickfix/fix41/ExecutionReport.h"
#include "quickfix/fix42/ExecutionReport.h"
#include "quickfix/fix43/ExecutionReport.h"
#include "quickfix/fix44/ExecutionReport.h"
#include "quickfix/fix50/ExecutionReport.h"

#include "Log.h"
#include "Toolkit.h"

void Application::onCreate( const FIX::SessionID& sessionID ) 
{
}
void Application::onLogon( const FIX::SessionID& sessionID )
{
}
void Application::onLogout( const FIX::SessionID& sessionID ) {}
void Application::toAdmin( FIX::Message& message,
                           const FIX::SessionID& sessionID ) 
{
}
void Application::toApp( FIX::Message& message,
                         const FIX::SessionID& sessionID )
throw( FIX::DoNotSend ) 
{

}

void Application::fromAdmin( const FIX::Message& message,
                             const FIX::SessionID& sessionID )
throw( FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::RejectLogon ) 
{
}

void Application::fromApp( const FIX::Message& message,
                           const FIX::SessionID& sessionID )
throw( FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::UnsupportedMessageType )
{ crack( message, sessionID ); }

void Application::onMessage( const FIX42::NewOrderSingle& message,
                             const FIX::SessionID& sessionID )
{
  FIX::SenderCompID senderCompId;
  FIX::OnBehalfOfCompID  onBehalfOfCompId;
  FIX::Account account;
  FIX::ClOrdID clOrdID;
  FIX::Symbol symbol;
  FIX::OrderQty orderQty;
  FIX::HandlInst handInst;
  FIX::OrdType ordType;
  FIX::Price price;
  FIX::Side side;
  FIX::OpenClose openClose;
  FIX::TransactTime transTime;
  FIX::TimeInForce timeInForce;
  //if ( ordType != FIX::OrdType_LIMIT )
  //  throw FIX::IncorrectTagValue( ordType.getField() );
  message.get( account );
  message.get( clOrdID );
  message.get( symbol );
  message.get( orderQty );
  message.get( handInst );
  message.get( ordType );
  message.get( price );
  message.get( side );


  //如果account全为数字，则表示客户显式指定了账户，直接通过账户获取对应的Api实例
  if (CToolkit::isAliasAcct(account.getValue()))
  {
  }
  else
  {

  }


  //如果account为账户别名，即包含字母，则要通过 SenderCompID + OnBehalfOfCompID + 别名 获取对应的Api实例
  std::string ssSenderCompId = message.getHeader().isSet(senderCompId) ? message.getHeader().get(senderCompId).getValue() : "";
  std::string ssOnBehalfOfCompID = message.getHeader().isSet(onBehalfOfCompId) ? 
    message.getHeader().get(onBehalfOfCompId).getValue() : "";



  //FIX42::ExecutionReport executionReport = FIX42::ExecutionReport
  //    ( FIX::OrderID( genOrderID() ),
  //      FIX::ExecID( genExecID() ),
  //      FIX::ExecTransType( FIX::ExecTransType_NEW ),
  //      FIX::ExecType( FIX::ExecType_FILL ),
  //      FIX::OrdStatus( FIX::OrdStatus_FILLED ),
  //      symbol,
  //      side,
  //      FIX::LeavesQty( 0 ),
  //      FIX::CumQty( orderQty ),
  //      FIX::AvgPx( price ) );

  //executionReport.set( clOrdID );
  //executionReport.set( orderQty );
  //executionReport.set( FIX::LastShares( orderQty ) );
  //executionReport.set( FIX::LastPx( price ) );

  //if( message.isSet(account) )
  //  executionReport.setField( message.get(account) );

  //try
  //{
  //  FIX::Session::sendToTarget( executionReport, sessionID );
  //}
  //catch ( FIX::SessionNotFound& ) {}
}

Application::Application(const CSgitApiManager &oSgitApiMngr)
   : m_orderID(0)
   , m_execID(0)
   , m_oSigtApiMngr(oSgitApiMngr)
{

}
