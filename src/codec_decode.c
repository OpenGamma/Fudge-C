/**
 * Copyright (C) 2009 - 2013, Vrai Stacey.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "fudge/envelope.h"
#include "fudge/string.h"
#include "codec_decode.h"
#include "fudge/header.h"
#include "memory_internal.h"
#include "registry_internal.h"
#include <assert.h>

fudge_i32 FudgeCodec_getNumBytes ( const FudgeTypeDesc * typedesc, fudge_i32 width )
{
    if ( ( typedesc->payload == FUDGE_TYPE_PAYLOAD_BYTES ) || ( typedesc->payload == FUDGE_TYPE_PAYLOAD_STRING ) )
        return typedesc->fixedwidth >= 0 ? typedesc->fixedwidth : width;
    else
        return 0;
}

FudgeStatus FudgeCodec_decodeField ( FudgeMsg message, FudgeFieldHeader header, fudge_i32 width, const fudge_byte * bytes, fudge_i32 numbytes )
{
    FudgeStatus status;
    FudgeTypeDecoder decoder;
    FudgeFieldData data;
    FudgeString name;
    const FudgeTypeDesc * typedesc = FudgeRegistry_getTypeDesc ( header.type );

    if ( width > numbytes )
        return FUDGE_OUT_OF_BYTES;

    /* If available for this type, use the registered decoder. Failing that,
       treat it as an array of bytes. */
    decoder = typedesc->decoder ? typedesc->decoder
                                : FudgeCodec_decodeFieldByteArray;

    memset ( &data, 0, sizeof ( data ) );

    if ( ( status = decoder ( bytes, width, &data ) ) != FUDGE_OK )
        return status;

    /* Construct the name string if required */
    if ( header.name )
    {
        if ( ( status = FudgeString_createFromUTF8 ( &name, header.name, header.namelen ) ) != FUDGE_OK )
            return status;
    }
    else
        name = 0;

    status = FudgeMsg_addFieldData ( message,
                                     header.type,
                                     name,
                                     FudgeHeader_getOrdinal ( &header ),
                                     &data,
                                     FudgeCodec_getNumBytes ( typedesc, width ) );
    if ( header.name )
        FudgeString_release ( name );
    return status;
}


FudgeStatus FudgeCodec_decodeMsgFields ( FudgeMsg message, const fudge_byte * bytes, fudge_i32 numbytes )
{
    FudgeStatus status;
    FudgeFieldHeader fieldheader;

    while ( numbytes )
    {
        fudge_i32 consumed, width;

        if ( ( status = FudgeHeader_decodeFieldHeader ( &fieldheader, &consumed, bytes, numbytes ) ) != FUDGE_OK )
            return status;
        bytes += consumed;
        numbytes -= consumed;

        /* Get the field width */
        if ( ( status = FudgeHeader_getFieldWidth ( &width, &consumed, fieldheader, bytes, numbytes ) ) != FUDGE_OK )
            goto release_fieldheader_and_fail;
        bytes += consumed;
        numbytes -= consumed;

        /* Get the field and add it to the message */
        if ( ( status = FudgeCodec_decodeField ( message, fieldheader, width, bytes, numbytes ) ) != FUDGE_OK )
            goto release_fieldheader_and_fail;
        bytes += width;
        numbytes -= width;

        /* Clean up this iteration */
        FudgeHeader_destroyFieldHeader ( fieldheader );
    }

    return FUDGE_OK;

/* Sorry Dijkstra */
release_fieldheader_and_fail:
    FudgeHeader_destroyFieldHeader ( fieldheader );
    return status;
}

/*****************************************************************************
 * Functions from fudge/codec_ex.h
 */

inline fudge_bool FudgeCodec_decodeBool ( const fudge_byte * bytes )
{
    return *bytes != 0;
}

inline fudge_byte FudgeCodec_decodeByte ( const fudge_byte * bytes )
{
    return *bytes;
}

#define FUDGECODEC_DECODE_PRIMITIVE_IMPL( type, typename, field, swapper )      \
    inline type FudgeCodec_decode##typename ( const fudge_byte * bytes )        \
    {                                                                           \
        return swapper ( *( ( type * ) bytes ) );                               \
    }

FUDGECODEC_DECODE_PRIMITIVE_IMPL( fudge_i16, I16, i16, ntohs )
FUDGECODEC_DECODE_PRIMITIVE_IMPL( fudge_i32, I32, i32, ntohl )
FUDGECODEC_DECODE_PRIMITIVE_IMPL( fudge_i64, I64, i64, ntohi64 )

#define FUDGECODEC_DECODE_PRIMITIVE_FP_IMPL( type, typename, field, swapper )   \
    inline type FudgeCodec_decode##typename ( const fudge_byte * bytes )        \
    {                                                                           \
        type value = *( ( type * ) bytes );                                     \
        return swapper ( value );                                               \
    }

FUDGECODEC_DECODE_PRIMITIVE_FP_IMPL( fudge_f32, F32, f32, ntohf )
FUDGECODEC_DECODE_PRIMITIVE_FP_IMPL( fudge_f64, F64, f64, ntohd )

inline FudgeStatus FudgeCodec_decodeByteArray ( const fudge_byte * data, const fudge_i32 width, fudge_byte * * bytes )
{
    if ( ! ( data && bytes ) )
        return FUDGE_NULL_POINTER;

    if ( width )
    {
        if ( ! ( *bytes = FUDGEMEMORY_MALLOC( fudge_byte *, width ) ) )
            return FUDGE_OUT_OF_MEMORY;
        memcpy ( *bytes, data, width );
    }
    else
        *bytes = 0;

    return FUDGE_OK;
}

inline FudgeStatus FudgeCodec_decodeString ( const fudge_byte * bytes, const fudge_i32 width, FudgeString * string )
{
    return FudgeString_createFromUTF8 ( string, bytes, width );
}

FudgeStatus FudgeCodec_decodeFieldByteArray ( const fudge_byte * bytes, const fudge_i32 width, FudgeFieldData * data )
{
    return FudgeCodec_decodeByteArray ( bytes, width, ( fudge_byte * * ) &( data->bytes ) );
}

FudgeStatus FudgeCodec_decodeFieldString ( const fudge_byte * bytes, const fudge_i32 width, FudgeFieldData * data )
{
    return FudgeCodec_decodeString ( bytes, width, &( data->string ) );
}

FudgeStatus FudgeCodec_decodeFieldIndicator ( const fudge_byte * bytes, const fudge_i32 width, FudgeFieldData * data )
{
    return FUDGE_OK;
}

FudgeStatus FudgeCodec_decodeFieldBool ( const fudge_byte * bytes, const fudge_i32 width, FudgeFieldData * data )
{
    data->boolean = FudgeCodec_decodeBool ( bytes );
    return FUDGE_OK;
}

FudgeStatus FudgeCodec_decodeFieldByte ( const fudge_byte * bytes, const fudge_i32 width, FudgeFieldData * data )
{
    data->byte = FudgeCodec_decodeByte ( bytes );
    return FUDGE_OK;
}

/*****************************************************************************
 * Functions from ./codec_decode.h
 */

#define FUDGECODEC_DECODE_PRIMITIVE_FIELD_IMPL( type, typename, field )                                                     \
    FudgeStatus FudgeCodec_decodeField##typename ( const fudge_byte * bytes, const fudge_i32 width, FudgeFieldData * data ) \
    {                                                                                                                       \
        data->field = FudgeCodec_decode##typename ( bytes );                                                                \
        return FUDGE_OK;                                                                                                    \
    }

FUDGECODEC_DECODE_PRIMITIVE_FIELD_IMPL( fudge_i16, I16, i16 )
FUDGECODEC_DECODE_PRIMITIVE_FIELD_IMPL( fudge_i32, I32, i32 )
FUDGECODEC_DECODE_PRIMITIVE_FIELD_IMPL( fudge_i64, I64, i64 )
FUDGECODEC_DECODE_PRIMITIVE_FIELD_IMPL( fudge_f32, F32, f32 )
FUDGECODEC_DECODE_PRIMITIVE_FIELD_IMPL( fudge_f64, F64, f64 )

#define FUDGECODEC_DECODE_ARRAY_FIELD_IMPL( type, typename, swapper )                                                               \
    FudgeStatus FudgeCodec_decodeField##typename##Array ( const fudge_byte * bytes, const fudge_i32 width, FudgeFieldData * data )  \
    {                                                                                                                               \
        type * target;                                                                                                              \
        const type * source = ( type * ) bytes;                                                                                     \
                                                                                                                                    \
        if ( width )                                                                                                                \
        {                                                                                                                           \
            type value;                                                                                                             \
            int index,                                                                                                              \
                numfields = width / sizeof ( type );                                                                                \
                                                                                                                                    \
            if ( ! ( target = FUDGEMEMORY_MALLOC( type *, width ) ) )                                                               \
                return FUDGE_OUT_OF_MEMORY;                                                                                         \
            for ( index = 0; index < numfields; ++index )                                                                           \
            {                                                                                                                       \
                value = source [ index ];                                                                                           \
                target [ index ] = swapper ( value );                                                                               \
            }                                                                                                                       \
        }                                                                                                                           \
        else                                                                                                                        \
            target = 0;                                                                                                             \
                                                                                                                                    \
        data->bytes = ( fudge_byte * ) target;                                                                                      \
        return FUDGE_OK;                                                                                                            \
    }

FUDGECODEC_DECODE_ARRAY_FIELD_IMPL( fudge_i16, I16, ntohs )
FUDGECODEC_DECODE_ARRAY_FIELD_IMPL( fudge_i32, I32, ntohl )
FUDGECODEC_DECODE_ARRAY_FIELD_IMPL( fudge_i64, I64, ntohi64 )
FUDGECODEC_DECODE_ARRAY_FIELD_IMPL( fudge_f32, F32, ntohf )
FUDGECODEC_DECODE_ARRAY_FIELD_IMPL( fudge_f64, F64, ntohd )

FudgeStatus FudgeCodec_decodeFieldFudgeMsg ( const fudge_byte * bytes, const fudge_i32 width, FudgeFieldData * data )
{
    FudgeMsg submessage;
    FudgeStatus status;

    if ( ( status = FudgeMsg_create ( &submessage ) ) != FUDGE_OK )
        return status;
    if ( ( status = FudgeCodec_decodeMsgFields ( submessage, bytes, width ) ) != FUDGE_OK )
    {
        FudgeMsg_release ( submessage );
        return status;
    }
    data->message = submessage;

    return FUDGE_OK;
}

FudgeStatus FudgeCodec_decodeFieldDate ( const fudge_byte * bytes, const fudge_i32 width, FudgeFieldData * data )
{
    FudgeDate * date = &( data->datetime.date );
    const fudge_i32 value = FudgeCodec_decodeI32 ( bytes );

    date->year = value >> 9;
    date->month = ( value >> 5 ) & 0xf;
    date->day = value & 0x1f;
    return FUDGE_OK;
}

FudgeStatus FudgeCodec_decodeFieldTime ( const fudge_byte * bytes, const fudge_i32 width, FudgeFieldData * data )
{
    FudgeTime * time = &( data->datetime.time );
    const fudge_i32 hiword = FudgeCodec_decodeI32 ( bytes );
    const fudge_i32 loword = FudgeCodec_decodeI32 ( bytes + sizeof ( fudge_i32 ) );
    int timezone;

    timezone = ( hiword ) >> 24;
    time->hasTimezone = ( timezone != -128 );
    time->timezoneOffset = time->hasTimezone ? timezone : 0;

    time->precision = ( hiword >> 20 ) & 0xf;
    time->seconds = hiword & 0x1ffff;
    time->nanoseconds = loword & 0x3fffffff;
    return FUDGE_OK;
}

FudgeStatus FudgeCodec_decodeFieldDateTime ( const fudge_byte * bytes, const fudge_i32 width, FudgeFieldData * data )
{
    FudgeStatus status;
    if ( ( status = FudgeCodec_decodeFieldDate ( bytes, width, data ) ) != FUDGE_OK )
        return status;
    return FudgeCodec_decodeFieldTime ( bytes + 4 /* Width of date representation */, width, data );
}

/*****************************************************************************
 * Functions from fudge/codec.h
 */

FudgeStatus FudgeCodec_decodeMsg ( FudgeMsgEnvelope * envelope, const fudge_byte * bytes, fudge_i32 numbytes )
{
    FudgeStatus status;
    FudgeMsgHeader header;
    FudgeMsg message;

    if ( ! envelope )
        return FUDGE_NULL_POINTER;

    /* Get the message header and use it to create the envelope and the message */
    if ( ( status = FudgeHeader_decodeMsgHeader ( &header, bytes, numbytes ) ) != FUDGE_OK )
        return status;
    if ( numbytes < header.numbytes )
        return FUDGE_OUT_OF_BYTES;
    if ( ( status = FudgeMsg_create ( &message ) ) != FUDGE_OK )
        return status;

    if ( ( status = FudgeMsgEnvelope_create ( envelope,
                                              header.directives,
                                              header.schemaversion,
                                              header.taxonomy,
                                              message ) ) != FUDGE_OK )
        goto release_message_and_fail;

    /* Envelope now has a message reference */
    FudgeMsg_release ( message );

    /* Advance to the end of the header */
    bytes += sizeof ( FudgeMsgHeader );
    numbytes -= sizeof ( FudgeMsgHeader );

    /* Consume fields */
    if ( ( status = FudgeCodec_decodeMsgFields ( message, bytes, numbytes ) ) != FUDGE_OK )
        goto release_envelope_and_fail;

    return status;

release_envelope_and_fail:
    FudgeMsgEnvelope_release ( *envelope );

release_message_and_fail:
    FudgeMsg_release ( message );
    return status;
}

