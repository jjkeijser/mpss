/*
 * Copyright (c) 2017, Intel Corporation.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
*/

#ifndef MICMGMT_FLASHSTATUS_HPP
#define MICMGMT_FLASHSTATUS_HPP

//  SYSTEM INCLUDES
//
#include    <cstdint>
#include    <memory>
#include    <string>

// NAMESPACE
//
namespace  micmgmt {


//----------------------------------------------------------------------------
//  CLASS:  FlashStatus

class  FlashStatus
{

public:

    enum  State  { eIdle, eBusy, eDone, eFailed, eAuthFailedVM, eStageComplete, eVerify, eUnknownFab };
    enum  ProgressStatus  { eWorking, eComplete, eProgressError, eVMError };


public:

    explicit FlashStatus( State state=eIdle, int progress=0, int stage=0, uint32_t error=0 );
    FlashStatus( const FlashStatus& that );
   ~FlashStatus();

    FlashStatus&  operator = ( const FlashStatus& that );
    bool          operator == ( const FlashStatus& that );

    bool          isBusy() const;
    bool          isError() const;
    bool          isCompleted() const;
    bool          isNewStage( const FlashStatus& that ) const;
    bool          isNewState( const FlashStatus& that ) const;
    bool          isNewProgress( const FlashStatus& that ) const;
    bool          isValidTransition( const FlashStatus& that ) const;
    int           progress() const;
    State         state() const;
    int           stage() const;
    std::string   stageText() const;
    std::string   errorText() const;
    uint32_t      errorCode() const;

    void          set( State state, int progress=0, int stage=0, uint32_t error=0 );
    void          setStageText( const std::string& text );
    void          setErrorText( const std::string& text );


private:

    struct  PrivData;
    std::unique_ptr<PrivData>  m_pData;

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

#endif // MICMGMT_FLASHSTATUS_HPP
