#include "../Common/Common.h"

#include "TwoButtonDynamicFilter.h"
#include "DasherInterfaceBase.h"
#include "Event.h"

#include <iostream>

// TODO: Move a lot of this stuff into a base class, so that the
// single and double button dynamic modes can behave in essentially
// the same way.


static SModuleSettings sSettings[] = {
  {LP_TWO_BUTTON_OFFSET, T_LONG, 1024, 2048, 2048, 100, "Button offset"},
  {LP_HOLD_TIME, T_LONG, 100, 10000, 1000, 100, "Long press time"},
  {LP_MULTIPRESS_TIME, T_LONG, 100, 10000, 1000, 100, "Multiple press time"},
  {LP_MULTIPRESS_COUNT,T_LONG, 2, 10, 1, 1, "Multiple press count"},
  {BP_BACKOFF_BUTTON,T_BOOL, -1, -1, -1, -1, "Enable backoff and start/stop buttons"},
  {BP_TWOBUTTON_REVERSE,T_BOOL, -1, -1, -1, -1, "Reverse up and down buttons"},
  {BP_SLOW_START,T_BOOL, -1, -1, -1, -1, "Slow startup"},
  {LP_SLOW_START_TIME, T_LONG, 0, 10000, 1000, 100, "Startup time"},
  {BP_TWOBUTTON_SPEED,T_BOOL, -1, -1, -1, -1, "Auto speed control"},
  {LP_DYNAMIC_MEDIAN_FACTOR, T_LONG, 10, 200, 100, 10, "Auto speed threshold"}
};

CTwoButtonDynamicFilter::CTwoButtonDynamicFilter(Dasher::CEventHandler * pEventHandler, CSettingsStore *pSettingsStore, CDasherInterfaceBase *pInterface, ModuleID_t iID, int iType, const char *szName)
  : CInputFilter(pEventHandler, pSettingsStore, pInterface, iID, iType, szName) { 
  //14, 1, "Two Button Dynamic Mode") {
  m_iState = 0;
  m_iLastTime = -1;
  m_bDecorationChanged = true;
  m_bKeyDown = false;

  m_pTree = new SBTree(2000);
  m_pTree->Add(2000);
  m_pTree->Add(2000);
}

CTwoButtonDynamicFilter::~CTwoButtonDynamicFilter() {
  if(m_pTree)
    delete m_pTree;
}

bool CTwoButtonDynamicFilter::DecorateView(CDasherView *pView) {
  CDasherScreen *pScreen(pView->Screen());

  CDasherScreen::point p[2];
  
  myint iDasherX;
  myint iDasherY;
  
  iDasherX = -100;
  iDasherY = 2048 - GetLongParameter(LP_TWO_BUTTON_OFFSET);
  
  pView->Dasher2Screen(iDasherX, iDasherY, p[0].x, p[0].y);
  
  iDasherX = -1000;
  iDasherY = 2048 - GetLongParameter(LP_TWO_BUTTON_OFFSET);
  
  pView->Dasher2Screen(iDasherX, iDasherY, p[1].x, p[1].y);
  
  pScreen->Polyline(p, 2, 3, 242);

  iDasherX = -100;
  iDasherY = 2048 + GetLongParameter(LP_TWO_BUTTON_OFFSET);
  
  pView->Dasher2Screen(iDasherX, iDasherY, p[0].x, p[0].y);
  
  iDasherX = -1000;
  iDasherY = 2048 + GetLongParameter(LP_TWO_BUTTON_OFFSET);
  
  pView->Dasher2Screen(iDasherX, iDasherY, p[1].x, p[1].y);
  
  pScreen->Polyline(p, 2, 3, 242);

  bool bRV(m_bDecorationChanged);
  m_bDecorationChanged = false;
  return bRV;
}

bool CTwoButtonDynamicFilter::Timer(int Time, CDasherView *m_pDasherView, CDasherModel *m_pDasherModel, Dasher::VECTOR_SYMBOL_PROB *pAdded, int *pNumDeleted) {
  if(m_bKeyDown && !m_bKeyHandled && ((Time - m_iKeyDownTime) > GetLongParameter(LP_HOLD_TIME))) {
    Event(Time, m_iHeldId, 1, m_pDasherModel, m_pUserLog);
    m_bKeyHandled = true;
    return true;
  }

  if(m_iState == 2)
    return m_pDasherModel->UpdatePosition(41943,2048, Time, pAdded, pNumDeleted);
  else if(m_iState == 1)
    return TimerImpl(Time, m_pDasherView, m_pDasherModel, pAdded, pNumDeleted);
  else
    return false;
}

bool CTwoButtonDynamicFilter::TimerImpl(int Time, CDasherView *m_pDasherView, CDasherModel *m_pDasherModel, Dasher::VECTOR_SYMBOL_PROB *pAdded, int *pNumDeleted) {
  return m_pDasherModel->UpdatePosition(100,2048, Time, pAdded, pNumDeleted);
}

void CTwoButtonDynamicFilter::KeyDown(int iTime, int iId, CDasherModel *pModel, CUserLogBase *pUserLog) {

  m_pUserLog = pUserLog;

  if(((iId == 0) || (iId == 1) || (iId == 100)) && !GetBoolParameter(BP_BACKOFF_BUTTON))
    return;

  if(m_bKeyDown)
    return;

  // Pass the basic key down event to the handler
  // TODO: bit of a hack here

  int iPreviousState = m_iState;
  Event(iTime, iId, 0, pModel, pUserLog);
  bool bStateChanged = m_iState != iPreviousState;
    
  // Store the key down time so that long presses can be determined
  // TODO: This is going to cause problems if multiple buttons are
  // held down at once
  m_iKeyDownTime = iTime;
  
  // Check for multiple clicks
  if(iId == m_iQueueId) {
    while((m_deQueueTimes.size() > 0) && (iTime - m_deQueueTimes.front()) > GetLongParameter(LP_HOLD_TIME))
      m_deQueueTimes.pop_front();

    if(m_deQueueTimes.size() == static_cast<unsigned int>(GetLongParameter(LP_MULTIPRESS_COUNT) - 1)) { 
      Event(iTime, iId, 2, pModel, pUserLog);
      m_deQueueTimes.clear();
    }
    else {
      if(!bStateChanged)
	m_deQueueTimes.push_back(iTime);
    }
  }
  else {
    if(!bStateChanged) {
      m_deQueueTimes.clear();
      m_deQueueTimes.push_back(iTime);
      m_iQueueId = iId;
    }
  }

  m_iHeldId = iId;
  m_bKeyDown = true;
  m_bKeyHandled = false;
}

void CTwoButtonDynamicFilter::KeyUp(int iTime, int iId, CDasherModel *pModel) {
  m_bKeyDown = false;
}

void CTwoButtonDynamicFilter::Activate() {
  SetBoolParameter(BP_DELAY_VIEW, true);
}

void CTwoButtonDynamicFilter::Deactivate() {
  SetBoolParameter(BP_DELAY_VIEW, false);
}

void CTwoButtonDynamicFilter::Event(int iTime, int iButton, int iType, CDasherModel *pModel, CUserLogBase *pUserLog) {
  // Types:
  // 0 = ordinary click
  // 1 = long click
  // 2 = multiple click
  
  if(iType == 2)
    if(GetBoolParameter(BP_TWOBUTTON_SPEED))
      AutoSpeedUndo(GetLongParameter(LP_MULTIPRESS_COUNT) - 1);
  
  // First sanity check - if Dasher is paused then jump to the
  // appropriate state
  if(GetBoolParameter(BP_DASHER_PAUSED))
    m_iState = 0;

  // TODO: Check that state diagram implemented here is what we
  // decided upon

  // What happens next depends on the state:
  switch(m_iState) {
  case 0: // Any button when paused causes a restart
    if(pUserLog)
      pUserLog->KeyDown(iButton, iType, 1);
    m_pInterface->Unpause(iTime);
    SetBoolParameter(BP_DELAY_VIEW, true);
    m_iState = 1;
    m_deQueueTimes.clear();
    break;
  case 1:
    switch(iType) {
    case 0:
      if((iButton == 0) || (iButton == 100)) {
	if(pUserLog)
	  pUserLog->KeyDown(iButton, iType, 2);
	m_iState = 0;
	m_deQueueTimes.clear();
	SetBoolParameter(BP_DELAY_VIEW, false);
	m_pInterface->PauseAt(0, 0);
      }
      else if(iButton == 1) {
	if(pUserLog)
	  pUserLog->KeyDown(iButton, iType, 6);
	SetBoolParameter(BP_DELAY_VIEW, false);
	m_iState = 2;
	m_deQueueTimes.clear();
      }
      else {
	ActionButton(iTime, iButton, iType, pModel, pUserLog);
      }
      break;
    case 1: // Delibarate fallthrough
    case 2: 
      if((iButton >= 2) && (iButton <= 4)) {
	if(pUserLog)
	  pUserLog->KeyDown(iButton, iType, 6);
	SetBoolParameter(BP_DELAY_VIEW, false);
	m_iState = 2;
	m_deQueueTimes.clear();
       }
      else {
	if(pUserLog)
	  pUserLog->KeyDown(iButton, iType, 0);
      }
      break;
    }
    break;
  case 2:
    if(pUserLog)
      pUserLog->KeyDown(iButton, iType, 2);
    
    m_iState = 0;
    m_deQueueTimes.clear();
    m_pInterface->PauseAt(0, 0);
    break;
  }

  m_iLastButton = iButton;
}

void CTwoButtonDynamicFilter::ActionButton(int iTime, int iButton, int iType, CDasherModel *pModel, CUserLogBase *pUserLog) {
  int iFactor(1);

  if(GetBoolParameter(BP_TWOBUTTON_REVERSE))
    iFactor = -1;

  if(iButton == 2) {
    pModel->Offset(iFactor * GetLongParameter(LP_TWO_BUTTON_OFFSET));
    if(pUserLog)
      pUserLog->KeyDown(iButton, iType, 3);
    if(GetBoolParameter(BP_TWOBUTTON_SPEED))
      AutoSpeedSample(iTime, pModel);
  }
  else if((iButton == 3) || (iButton == 4)) {
    pModel->Offset(iFactor * -GetLongParameter(LP_TWO_BUTTON_OFFSET));
    if(pUserLog)
      pUserLog->KeyDown(iButton, iType, 4);
    if(GetBoolParameter(BP_TWOBUTTON_SPEED))
      AutoSpeedSample(iTime, pModel);
  }
  else {
    if(pUserLog)
      pUserLog->KeyDown(iButton, iType, 0);
  }
}

bool CTwoButtonDynamicFilter::GetSettings(SModuleSettings **pSettings, int *iCount) {
  *pSettings = sSettings;
  *iCount = sizeof(sSettings) / sizeof(SModuleSettings);

  return true;
};

bool CTwoButtonDynamicFilter::GetMinWidth(int &iMinWidth) {
  iMinWidth = 1024;
  return true;
}

void CTwoButtonDynamicFilter::AutoSpeedSample(int iTime, CDasherModel *pModel) {
  if(m_iLastTime == -1) {
    m_iLastTime = iTime;
    return;
  }

  if(pModel->IsSlowdown(iTime))
    return;

  int iDiff(iTime - m_iLastTime);
  m_iLastTime = iTime;

  if(m_pTree) {
    int iMedian(m_pTree->GetOffset(m_pTree->GetCount() / 2));
    
    if((iDiff <= 300) || (iDiff < (iMedian * GetLongParameter(LP_DYNAMIC_MEDIAN_FACTOR)) / 100)) {
      pModel->TriggerSlowdown();
    }
  }
  
  if(!m_pTree)
    m_pTree = new SBTree(iDiff);
  else
    m_pTree->Add(iDiff);

  m_deOffsetQueue.push_back(iDiff);

  while(m_deOffsetQueue.size() > 10) {
    m_pTree = m_pTree->Delete(m_deOffsetQueue.front());
    m_deOffsetQueue.pop_front();
  }
}

void CTwoButtonDynamicFilter::AutoSpeedUndo(int iCount) {
  for(int i(0); i < iCount; ++i) {
    if(m_deOffsetQueue.size() == 0)
      return;

    if(m_pTree)
      m_pTree = m_pTree->Delete(m_deOffsetQueue.back());
    m_deOffsetQueue.pop_back();
  }
}

CTwoButtonDynamicFilter::SBTree::SBTree(int iValue) {
  m_iValue = iValue;
  m_pLeft = NULL;
  m_pRight = NULL;
  m_iCount = 1;
}

CTwoButtonDynamicFilter::SBTree::~SBTree() {
  if(m_pLeft)
    delete m_pLeft;

  if(m_pRight)
    delete m_pRight;
}

void CTwoButtonDynamicFilter::SBTree::Add(int iValue) {
  ++m_iCount;

  if(iValue > m_iValue) {
    if(m_pRight)
      m_pRight->Add(iValue);
    else
      m_pRight = new SBTree(iValue);
  }
  else {
    if(m_pLeft)
      m_pLeft->Add(iValue);
    else
      m_pLeft = new SBTree(iValue);
  }
}

CTwoButtonDynamicFilter::SBTree* CTwoButtonDynamicFilter::SBTree::Delete(int iValue) {
  // Hmm... deleting is awkward in binary trees

  if(iValue == m_iValue) {
    if(!m_pLeft) {
      SBTree *pOldRight = m_pRight;
      m_pRight = NULL;
      delete this;
      return pOldRight;
    }
    else {
      SBTree *pOldLeft = m_pLeft;
      pOldLeft->SetRightMost(m_pRight);
      m_pLeft = NULL;
      m_pRight = NULL;
      delete this;
      return pOldLeft;
    }
  }
  else if(iValue > m_iValue) {
    --m_iCount;
    m_pRight = m_pRight->Delete(iValue);
  }
  else {
    --m_iCount;
    m_pLeft = m_pLeft->Delete(iValue);
  }

  return this;
}

void CTwoButtonDynamicFilter::SBTree::SetRightMost(CTwoButtonDynamicFilter::SBTree* pNewTree) {
  if(pNewTree)
    m_iCount += pNewTree->GetCount();

  if(m_pRight)
    m_pRight->SetRightMost(pNewTree);
  else
    m_pRight = pNewTree;
}

int CTwoButtonDynamicFilter::SBTree::GetOffset(int iOffset) {
  if(m_pLeft && (m_pLeft->GetCount() > iOffset))
    return m_pLeft->GetOffset(iOffset);
  else if((m_pLeft && (m_pLeft->GetCount() == iOffset)) || (!m_pLeft && (iOffset == 0)))
    return m_iValue;
  else if(m_pLeft)
    return m_pRight->GetOffset(iOffset - m_pLeft->GetCount() - 1);
  else
    return m_pRight->GetOffset(iOffset - 1);
}