// DasherNode.cpp
//
// Copyright (c) 2007 David Ward
//
// This file is part of Dasher.
//
// Dasher is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Dasher is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Dasher; if not, write to the Free Software 
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#include "../Common/Common.h"

#include "AlphabetManager.h"

#include "DasherNode.h"

using namespace Dasher;
using namespace Opts;
using namespace std;

// Track memory leaks on Windows to the line that new'd the memory
#ifdef _WIN32
#ifdef _DEBUG
#define DEBUG_NEW new( _NORMAL_BLOCK, THIS_FILE, __LINE__ )
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif
#endif

// TODO: put this back to being inlined
CDasherNode::~CDasherNode() {
  //  std::cout << "Deleting node: " << this << std::endl;
  // Release any storage that the node manager has allocated,
  // unreference ref counted stuff etc.
  Delete_children();

  m_pNodeManager->ClearNode( this );
  m_pNodeManager->Unref();

  //  std::cout << "done." << std::endl;

  delete m_pDisplayInfo;
}


void CDasherNode::Trace() const {
  /* TODO sort out
     dchar out[256];
     if (m_Symbol)
     wsprintf(out,TEXT("%7x %3c %7x %5d %7x  %5d %8x %8x      \n"),this,m_Symbol,m_iGroup,m_context,m_Children,m_Cscheme,m_iLbnd,m_iHbnd);
     else
     wsprintf(out,TEXT("%7x     %7x %5d %7x  %5d %8x %8x      \n"),this,m_iGroup,m_context,m_Children,m_Cscheme,m_iLbnd,m_iHbnd);

     OutputDebugString(out);

     if (m_Children) {
     unsigned int i; 
     for (i=1;i<m_iChars;i++)
     m_Children[i]->Dump_node();
     }
   */
}

bool CDasherNode::NodeIsParent(CDasherNode *oldnode) const {
  if(oldnode == m_pParent)
    return true;
  else
    return false;

}

CDasherNode *const CDasherNode::Get_node_under(int iNormalization, myint miY1, myint miY2, myint miMousex, myint miMousey) {
  myint miRange = miY2 - miY1;

  // TODO: Manipulating flags in a 'get' method?
  SetFlag(NF_ALIVE, true);

  ChildMap::const_iterator i;
  for(i = GetChildren().begin(); i != GetChildren().end(); i++) {
    CDasherNode *pChild = *i;

    myint miNewy1 = miY1 + (miRange * pChild->m_iLbnd) / iNormalization;
    myint miNewy2 = miY1 + (miRange * pChild->m_iHbnd) / iNormalization;
    if(miMousey < miNewy2 && miMousey > miNewy1 && miMousex < miNewy2 - miNewy1)
      return pChild->Get_node_under(iNormalization, miNewy1, miNewy2, miMousex, miMousey);
  }
  return this;
}

// kill ourselves and all other children except for the specified
// child
// FIXME this probably shouldn't be called after history stuff is working
void CDasherNode::OrphanChild(CDasherNode *pChild) {
  DASHER_ASSERT(ChildCount() > 0);

  ChildMap::const_iterator i;
  for(i = GetChildren().begin(); i != GetChildren().end(); i++) {
    if((*i) != pChild) {
      (*i)->Delete_children();
      delete (*i);
    }
  }
  
  pChild->SetParent(NULL);

  Children().clear();
  SetFlag(NF_ALLCHILDREN, false);
}

// Delete nephews of the child which has the specified symbol
// TODO: Need to allow for subnode
void CDasherNode::DeleteNephews(CDasherNode *pChild) {
  DASHER_ASSERT(Children().size() > 0);

  ChildMap::iterator i;
  for(i = Children().begin(); i != Children().end(); i++) {
    if((*i)->GetFlag(NF_SUBNODE))
      (*i)->DeleteNephews(pChild);
    else {
      if(*i != pChild) {
	(*i)->Delete_children();
      }
    }
  }
}

// TODO: Need to allow for subnodes
// TODO: Incorporate into above routine
void CDasherNode::Delete_children() {
//    CAlphabetManager::SAlphabetData *pParentUserData(static_cast<CAlphabetManager::SAlphabetData *>(m_pUserData));

//    if((GetDisplayInfo()->strDisplayText)[0] == 'e')
//      std::cout << "ed: " << this << " " << pParentUserData->iContext << " " << pParentUserData->iOffset << std::endl;

//  std::cout << "Start: " << this << std::endl;

  ChildMap::iterator i;
  for(i = Children().begin(); i != Children().end(); i++) {
    //    std::cout << "CNM: " << (*i)->m_pNodeManager << " (" << (*i)->m_pNodeManager->GetID() << ") " << (*i) << " " << (*i)->Parent() << std::endl;
    delete (*i);
  }
  Children().clear();
  //  std::cout << "NM: " << m_pNodeManager << std::endl;
  SetFlag(NF_ALLCHILDREN, false);
}

// Gets the probability of this node, conditioned on the parent
double CDasherNode::GetProb(int iNormalization) {    
  return (double) (m_iHbnd - m_iLbnd) / (double) iNormalization;
}

void CDasherNode::ConvertWithAncestors() {
  if(GetFlag(NF_CONVERTED))
    return;
  
  SetFlag(NF_CONVERTED, true);

  if(m_pParent)
    m_pParent->ConvertWithAncestors();
}

void CDasherNode::SetFlag(int iFlag, bool bValue) {
  if(bValue)
    m_iFlags = m_iFlags | iFlag;
  else
    m_iFlags = m_iFlags & (~iFlag);
  
  m_pNodeManager->SetFlag(this, iFlag, bValue);
}
 
void CDasherNode::SetParent(CDasherNode *pNewParent) {
  m_pParent = pNewParent;
}

int CDasherNode::MostProbableChild() {
  int iMax(0);
  int iCurrent;
  
  for(ChildMap::iterator it(m_mChildren.begin()); it != m_mChildren.end(); ++it) {
    iCurrent = (*it)->Range();
    
    if(iCurrent > iMax)
      iMax = iCurrent;
  }
  
  return iMax;
}