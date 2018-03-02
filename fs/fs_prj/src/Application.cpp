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

#include "quickfix/fix42/BusinessMessageReject.h"

#include "Log.h"
#include "Toolkit.h"

void Application::onCreate( const FIX::SessionID& sessionID ) 
{
	LOG(INFO_LOG_LEVEL, "%s", sessionID.toString().c_str());
}
void Application::onLogon( const FIX::SessionID& sessionID )
{
	LOG(INFO_LOG_LEVEL, "%s", sessionID.toString().c_str());
  if (m_pSigtCtx) m_pSigtCtx->UpsertLoginStatus(sessionID.toString(), true);
}
void Application::onLogout( const FIX::SessionID& sessionID ) 
{
	LOG(INFO_LOG_LEVEL, "%s", sessionID.toString().c_str());
  if (m_pSigtCtx) m_pSigtCtx->UpsertLoginStatus(sessionID.toString(), false);

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
}

void Application::fromApp( const FIX::Message& message,
                           const FIX::SessionID& sessionID )
throw( FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::UnsupportedMessageType )
{
	LOG(INFO_LOG_LEVEL, "%s", message.toString().c_str());

  std::string ssErr = "";
  bool isNeedSendBusinessReject = false;
  if (!m_pSigtCtx)
  {
    ssErr = "m_pSigtCtx is invalid";
    LOG(ERROR_LOG_LEVEL, ssErr.c_str());
    isNeedSendBusinessReject = true;
  }
  else if(!m_pSigtCtx->Deal(message, sessionID, ssErr))
  {
    isNeedSendBusinessReject = true;
  }
  

  if (isNeedSendBusinessReject)
  {
    FIX42::BusinessMessageReject oBusinessReject = FIX42::BusinessMessageReject(message.getHeader().getField(FIX::FIELD::MsgType), FIX::BusinessRejectReason_APPLICATION_NOT_AVAILABLE);
    oBusinessReject.set(FIX::Text(ssErr));
    FIX::Session::sendToTarget(oBusinessReject, sessionID);
  }
}

void Application::onMessage(const FIX42::Logon& oMsg, const FIX::SessionID& oSessionID)
{
  FIX::SenderSubID senderSubID;
  FIX::RawData rawData;

  oMsg.getHeader().getIfSet(senderSubID);  
  oMsg.getIfSet(rawData);

  //没在指定tag带上值属于不需要进行登录验证的情况
  if (senderSubID.getValue().empty() || rawData.getValue().empty()) return;
  LOG(INFO_LOG_LEVEL, "senderSubID:%s,rawData:%s", senderSubID.getValue().c_str(), rawData.getValue().c_str());

  if (!m_pSigtCtx->IsTradeSupported())
  {
    throw FIX::RejectLogon("Trade request is not support on this fix gatew");
  }
  //如果没有找到对应的api实例，创建实例，并登录。如果找到，直接登录
  SharedPtr<CSgitTdSpi> spTdSpi = NULL;
  std::string ssErrMsg = "";
  if (m_pSigtCtx)
  {
    spTdSpi = m_pSigtCtx->GetOrCreateTdSpi(oSessionID, CSgitTdSpi::Direct);
  }
  else
  {
    ssErrMsg = "CSgitContext is invalid";
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
