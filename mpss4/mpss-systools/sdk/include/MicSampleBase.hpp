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

#ifndef MICMGMT_MICSAMPLEBASE_HPP
#define MICMGMT_MICSAMPLEBASE_HPP

// SYSTEM INCLUDES
//
#include    <cstdint>
#include    <string>

// NAMESPACE
//
namespace  micmgmt
{


//============================================================================
//  CLASS:  MicSampleBase

class  MicSampleBase    // Abstract
{

public:

    enum  Exponent
    {
        eGiga=9,
        eMega=6,
        eKilo=3,
        eBase=0,
        eMilli=-3,
        eMicro=-6,
        eNano=-9
    };


public:

    virtual ~MicSampleBase();

    MicSampleBase&    operator =  ( uint32_t sample );
    MicSampleBase&    operator =  ( const MicSampleBase& that );

    bool              isValid() const;
    bool              isAvailable() const;
    bool              isLowerThresholdViolation() const;
    bool              isUpperThresholdViolation() const;
    int               compare( const MicSampleBase& that ) const;
    uint32_t          rawSample() const;
    uint32_t          dataMask() const;
    int               exponent() const;
    std::string       name() const;
    size_t            nameLength() const;
    std::string       unit() const;
    int               value() const;
    double            normalizedValue() const;
    double            scaledValue( int exponent=eBase ) const;

    void              clearValue();
    void              setExponent( int exponent );
    void              setName( const std::string& name );


protected:

    MicSampleBase();
    explicit MicSampleBase( uint32_t sample, int exponent=eBase );
    MicSampleBase( const std::string& name, uint32_t sample, int exponent=eBase );
    MicSampleBase( const MicSampleBase& that );

    virtual double       scaled( double value, int exponent ) const;
    virtual uint32_t     impDataMask() const;
    virtual uint32_t     impStatusMask() const;
    virtual int          impValue() const = 0;
    virtual std::string  impUnit() const = 0;


private:

    std::string  m_name;
    uint32_t     m_sample;
    int          m_exponent;
    bool         m_valid;

};

//----------------------------------------------------------------------------

}   // namespace micmgmt

//----------------------------------------------------------------------------

#endif // MICMGMT_MICSAMPLEBASE_HPP
