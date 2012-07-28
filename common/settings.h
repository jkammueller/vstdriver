#include <cstdint>
#include <cstring>
#include <vector>
#include "../driver/src/inc/aeffect.h"

template<typename T>
static void append_be( std::vector<std::uint8_t> & out, const T & value )
{
	union
	{
		T original;
		std::uint8_t raw[sizeof(T)];
	} carriage;
	carriage.original = value;
	for ( unsigned i = 0; i < sizeof(T); ++i )
	{
		out.push_back( carriage.raw[ sizeof(T) - 1 - i ] );
	}
}

template<typename T>
static void retrieve_be( T & out, const std::uint8_t * & in, unsigned & size )
{
	if ( size < sizeof(T) ) return;

	size -= sizeof(T);

	union
	{
		T original;
		std::uint8_t raw[sizeof(T)];
	} carriage;
	for ( unsigned i = 0; i < sizeof(T); ++i )
	{
		carriage.raw[ sizeof(T) - 1 - i ] = *in++;
	}

	out = carriage.original;
}

static void getChunk( AEffect * pEffect, std::vector<std::uint8_t> & out )
{
	out.resize( 0 );
	std::uint32_t unique_id = pEffect->uniqueID;
	append_be( out, unique_id );
	bool type_chunked = !!( pEffect->flags & effFlagsProgramChunks );
	append_be( out, type_chunked );
	if ( !type_chunked )
	{
		std::uint32_t num_params = pEffect->numParams;
		append_be( out, num_params );
		for ( unsigned i = 0; i < num_params; ++i ) 
		{
			float parameter = pEffect->getParameter( pEffect, i );
			append_be( out, parameter );
		}
	}
	else
	{
		void * chunk;
		std::uint32_t size = pEffect->dispatcher( pEffect, effGetChunk, 0, 0, &chunk, 0 );
		append_be( out, size );
		size_t chunk_size = out.size();
		out.resize( chunk_size + size );
		std::memcpy( &out[ chunk_size ], chunk, size );
	}
}

static void setChunk( AEffect * pEffect, std::vector<std::uint8_t> const& in )
{
	unsigned size = in.size();
	if ( pEffect && size )
	{
		const std::uint8_t * inc = in.data();
		std::uint32_t effect_id;
		retrieve_be( effect_id, inc, size );
		if ( effect_id != pEffect->uniqueID ) return;
		bool type_chunked;
		retrieve_be( type_chunked, inc, size );
		if ( type_chunked != !!( pEffect->flags & effFlagsProgramChunks ) ) return;
		if ( !type_chunked )
		{
			std::uint32_t num_params;
			retrieve_be( num_params, inc, size );
			if ( num_params != pEffect->numParams ) return;
			for ( unsigned i = 0; i < num_params; ++i )
			{
				float parameter;
				retrieve_be( parameter, inc, size );
				pEffect->setParameter( pEffect, i, parameter );
			}
		}
		else
		{
			std::uint32_t chunk_size;
			retrieve_be( chunk_size, inc, size );
			if ( chunk_size > size ) return;
			pEffect->dispatcher( pEffect, effSetChunk, 0, chunk_size, ( void * ) inc, 0 );
		}
	}
}
