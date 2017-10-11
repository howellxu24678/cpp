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
  LOG(INFO_LOG_LEVEL, "%s", message.toString().c_str());
}

void Application::fromAdmin( const FIX::Message& message,
                             const FIX::SessionID& sessionID )
throw( FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::RejectLogon ) 
{
}

void Application::fromApp( const FIX::Message& message,
                           const FIX::SessionID& sessionID )
throw( FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::UnsupportedMessageType )
{
	LOG(INFO_LOG_LEVEL, "%s", message.toString().c_str());
  m_pSigtCtx->AddFixInfo(message, sessionID);
	crack( message, sessionID ); 
}

void Application::onMessage(const FIX42::NewOrderSingle& oMsg, const FIX::SessionID& oSessionID)
{
	SharedPtr<CSgitTradeSpi> spTradeSpi = m_pSigtCtx->GetSpi(oMsg);
	if (spTradeSpi)
	{
		spTradeSpi->ReqOrderInsert(oMsg);
	}
}

void Application::onMessage(const FIX42::OrderCancelRequest& oMsg, const FIX::SessionID& oSessionID)
{
  SharedPtr<CSgitTradeSpi> spTradeSpi = m_pSigtCtx->GetSpi(oMsg);
  if (spTradeSpi)
  {
    spTradeSpi->ReqOrderAction(oMsg);
  }
}

void Application::onMessage(const FIX42::OrderStatusRequest& oMsg, const FIX::SessionID& oSessionID)
{
  SharedPtr<CSgitTradeSpi> spTradeSpi = m_pSigtCtx->GetSpi(oMsg);
  if (spTradeSpi)
  {
    spTradeSpi->ReqQryOrder(oMsg);
  }
}

Application::Application(CSgitContext* pSgitCtx)
   : m_pSigtCtx(pSgitCtx)
{

}
