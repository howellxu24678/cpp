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
	LOG(INFO_LOG_LEVEL, "%s", sessionID.toString().c_str());
}
void Application::onLogon( const FIX::SessionID& sessionID )
{
	LOG(INFO_LOG_LEVEL, "%s", sessionID.toString().c_str());
}
void Application::onLogout( const FIX::SessionID& sessionID ) 
{
	LOG(INFO_LOG_LEVEL, "%s", sessionID.toString().c_str());

  SharedPtr<CSgitTdSpi> spTdSpi = NULL;
  if (m_pSigtCtx && (spTdSpi = m_pSigtCtx->GetTdSpi(sessionID)))
  {
    spTdSpi->ReqUserLogout();
  }
}
void Application::toAdmin( FIX::Message& message,
                           const FIX::SessionID& sessionID ) 
{
}
void Application::toApp( FIX::Message& message,
                         const FIX::SessionID& sessionID )
throw( FIX::DoNotSend ) 
{
  LOG(INFO_LOG_LEVEL, "%s", message.toString().c_str());
}

void Application::fromAdmin( const FIX::Message& message,
                             const FIX::SessionID& sessionID )
throw( FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::RejectLogon ) 
{
  crack( message, sessionID ); 
  //const std::string& msgTypeValue 
  //  = message.getHeader().getField( FIX::FIELD::MsgType );

  //if( msgTypeValue == FIX::MsgType_Logon )
  //{
  //  FIX::RawData rawData;
  //  message.getFieldIfSet(rawData);

  //  if (rawData.getValue().empty()) return;

  //  LOG(INFO_LOG_LEVEL, "rawData:%s", rawData.getValue().c_str());

  //  //throw FIX::DoNotSend();
  //}
}

void Application::fromApp( const FIX::Message& message,
                           const FIX::SessionID& sessionID )
throw( FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::UnsupportedMessageType )
{
	LOG(INFO_LOG_LEVEL, "%s", message.toString().c_str());

  if (m_pSigtCtx) m_pSigtCtx->Deal(message, sessionID);

 // m_pSigtCtx->AddFixInfo(message);
	//crack( message, sessionID ); 
}

void Application::onMessage(const FIX42::Logon& oMsg, const FIX::SessionID& oSessionID)
{
  FIX::SenderSubID senderSubID;
  FIX::RawData rawData;

  oMsg.getHeader().getIfSet(senderSubID);  
  oMsg.getIfSet(rawData);

  if (senderSubID.getValue().empty() || rawData.getValue().empty()) return;
  LOG(INFO_LOG_LEVEL, "senderSubID:%s,rawData:%s", senderSubID.getValue().c_str(), rawData.getValue().c_str());

  //如果没有找到对应的api实例，创建实例，并登录。如果找到，直接登录
  SharedPtr<CSgitTdSpi> spTdSpi = NULL;
  std::string ssErrMsg = "";
  if (m_pSigtCtx)
  {
    spTdSpi = m_pSigtCtx->GetOrCreateTdSpi(oSessionID, CSgitTdSpi::Direct);
  }
  else
  {
    ssErrMsg = "CSgitContext is unvalid";
    throw FIX::RejectLogon(ssErrMsg);
  }

  //登录成功，返回，此时会回复正常登录应答
  if (spTdSpi && spTdSpi->ReqUserLogin(senderSubID.getValue(), rawData.getValue(), ssErrMsg)) return;

  //其他情况返回登录失败
  throw FIX::RejectLogon(ssErrMsg);
}

//void Application::onMessage(const FIX42::NewOrderSingle& oMsg, const FIX::SessionID&)
//{
//	SharedPtr<CSgitTdSpi> spTdSpi = m_pSigtCtx->GetTdSpi(oMsg);
//	if (spTdSpi)
//	{
//		spTdSpi->ReqOrderInsert(oMsg);
//	}
//}
//
//void Application::onMessage(const FIX42::OrderCancelRequest& oMsg, const FIX::SessionID&)
//{
//  SharedPtr<CSgitTdSpi> spTdSpi = m_pSigtCtx->GetTdSpi(oMsg);
//  if (spTdSpi)
//  {
//    spTdSpi->ReqOrderAction(oMsg);
//  }
//}
//
//void Application::onMessage(const FIX42::OrderStatusRequest& oMsg, const FIX::SessionID&)
//{
//  SharedPtr<CSgitTdSpi> spTdSpi = m_pSigtCtx->GetTdSpi(oMsg);
//  if (spTdSpi)
//  {
//    spTdSpi->ReqQryOrder(oMsg);
//  }
//}
//
//void Application::onMessage(const FIX42::MarketDataRequest& oMsg, const FIX::SessionID&)
//{
//  SharedPtr<CSgitMdSpi> spMdSpi = m_pSigtCtx->GetMdSpi(oMsg);
//  if (spMdSpi)
//  {
//    spMdSpi->MarketDataRequest(oMsg);
//  }
//}

Application::Application(CSgitContext* pSgitCtx)
   : m_pSigtCtx(pSgitCtx)
{

}
