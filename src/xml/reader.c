/*!The Tiny Box Library
 * 
 * TBox is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * TBox is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with TBox; 
 * If not, see <a href="http://www.gnu.org/licenses/"> http://www.gnu.org/licenses/</a>
 * 
 * Copyright (C) 2009 - 2010, ruki All rights reserved.
 *
 * \author		ruki
 * \file		reader.c
 *
 */

/* /////////////////////////////////////////////////////////
 * includes
 */
#include "reader.h"


/* /////////////////////////////////////////////////////////
 * macros
 */
#ifndef TPLAT_CONFIG_COMPILER_NOT_SUPPORT_VARARG_MACRO
#if 1
# 	define TB_READER_DBG(fmt, arg...) 			TB_DBG("[xml]: " fmt, ##arg)
#else
# 	define TB_READER_DBG(fmt, arg...)
#endif

#else
# 	define TB_READER_DBG
#endif


/* /////////////////////////////////////////////////////////
 * details
 */

static tb_char_t tb_xml_reader_get_char(tb_xml_reader_t* reader)
{
	// get character from cache
	if (reader->cache != '\0') 
	{
		tb_char_t ch = reader->cache;
		reader->cache = '\0';
		return ch;
	}

	// get character
	tb_char_t ch[1];
	if (1 == tb_stream_read(reader->st, ch, 1)) return ch[0];
	else return '\0';
}
static tb_char_t tb_xml_reader_peek_char(tb_xml_reader_t* reader)
{
	// get character from cache
	if (reader->cache != '\0') return reader->cache;

	// map character 
	tb_char_t const* p = tb_stream_need(reader->st, 1);

	// get character
	if (p) return *p;
	else return '\0';
}
static void tb_xml_reader_seek_char(tb_xml_reader_t* reader)
{
	tb_stream_seek(reader->st, 1, TB_STREAM_SEEK_CUR);
}
static tb_char_t const* tb_xml_reader_parse_element(tb_xml_reader_t* reader)
{
	// clear element
	tb_string_clear(&reader->element);

	// parse element
	tb_char_t ch = '\0';
	tb_size_t in = 0;
	while (ch = tb_xml_reader_get_char(reader))
	{
		// appedn element
		if (!in && ch == '<') in = 1;
		else if (in)
		{
			if (ch != '>') tb_string_append_char(&reader->element, ch);
			else return tb_string_c_string(&reader->element);
		}
	}
	return TB_NULL;
}
static tb_char_t const* tb_xml_reader_parse_text(tb_xml_reader_t* reader)
{
	// clear text
	tb_string_clear(&reader->text);

	// parse text
	tb_char_t ch = '\0';
	tb_size_t in = 0;
	while (ch = tb_xml_reader_get_char(reader))
	{
		// append text
		if (ch != '<') tb_string_append_char(&reader->text, ch);
		else 
		{
			reader->cache = ch;
			return tb_string_c_string(&reader->text);
		}
	}
	return TB_NULL;
}
static tb_char_t const* tb_xml_reader_parse_keyvalue(tb_string_t const* data, tb_char_t const* key, tb_string_t* value)
{
	TB_ASSERT(data && key);
	if (!data || !key || !value) return TB_NULL;

	// find key
	tb_int_t pos = tb_string_find_c_string(data, key, 0);
	if (pos < 0) return TB_NULL;

	// find the begin: "
	tb_int_t pos_b = tb_string_find_c_string(data, "\"", pos + strlen(key));
	if (pos_b < 0) return TB_NULL;

	// find the end: "
	tb_int_t pos_e = tb_string_find_c_string(data, "\"", ++pos_b);
	if (pos_e < 0) return TB_NULL;

	// get value
	if (TB_FALSE == tb_string_subat(data, value, pos_b, pos_e - pos_b))
		return TB_NULL;

	return tb_string_c_string(value);
}
/* /////////////////////////////////////////////////////////
 * interfaces: open & close
 */

tb_xml_reader_t* tb_xml_reader_open(tb_stream_t* st)
{
	TB_ASSERT(st);
	if (!st) return TB_NULL;

	// alloc reader
	tb_xml_reader_t* reader = (tb_xml_reader_t*)tb_malloc(sizeof(tb_xml_reader_t));
	if (!reader) return TB_NULL;

	// init it
	memset(reader, 0, sizeof(tb_xml_reader_t));
	reader->st = st;
	reader->event = TB_XML_READER_EVENT_NULL;
	reader->cache = '\0';

	// init string
	tb_string_init(&reader->version);
	tb_string_init(&reader->encoding);
	tb_string_init(&reader->element);
	tb_string_init(&reader->name);
	tb_string_init(&reader->text);

	// reader the first event
	tb_xml_reader_next(reader);

	return reader;
}

void tb_xml_reader_close(tb_xml_reader_t* reader)
{
	if (reader)
	{
		// detach stream
		reader->st = TB_NULL;

		// free element
		tb_string_uninit(&reader->name);
	
		// free element
		tb_string_uninit(&reader->element);
	
		// free text
		tb_string_uninit(&reader->text);

		// free element
		tb_string_uninit(&reader->version);
	
		// free text
		tb_string_uninit(&reader->encoding);

		// free it
		tb_free(reader);
	}
}

/* /////////////////////////////////////////////////////////
 * interfaces: iterator
 */
tb_bool_t tb_xml_reader_has_next(tb_xml_reader_t* reader)
{
	if (reader && reader->event != TB_XML_READER_EVENT_NULL)
		return TB_TRUE;
	else return TB_FALSE;
}
tb_size_t tb_xml_reader_next(tb_xml_reader_t* reader)
{
	TB_ASSERT(reader);
	if (!reader) return TB_XML_READER_EVENT_NULL;

	// reset event
	reader->event = TB_XML_READER_EVENT_NULL;

	// peek a character
	tb_char_t ch = tb_xml_reader_peek_char(reader);

	// is element?
	if (ch == '<') 
	{
		// parse element: <...>
		tb_char_t const* element = tb_xml_reader_parse_element(reader);
		if (!element) goto end;

		// is document begin: <?xml version="..." encoding=".." ?>
		tb_size_t size = tb_string_size(&reader->element);
		if (size > 4 
			&& element[0] == '?'
			&& element[1] == 'x'
			&& element[2] == 'm'
			&& element[3] == 'l')
		{
			// parse version & encoding
			tb_char_t const* version = tb_xml_reader_parse_keyvalue(&reader->element, "version", &reader->version);
			tb_char_t const* encoding = tb_xml_reader_parse_keyvalue(&reader->element, "encoding", &reader->encoding);
			TB_ASSERT(version && encoding);
			if (!version || !encoding) goto end;

			// seek to the first element
			tb_xml_reader_parse_text(reader);

			// update event
			reader->event = TB_XML_READER_EVENT_DOCUMENT_BEG;
		}
		// is element end: </name>
		else if (size > 1 && element[0] == '/')
		{
			// update event
			reader->event = TB_XML_READER_EVENT_ELEMENT_END;
		}
		// is comment: <!-- text -->
		else if (size > 5
			&& element[0] == '!' && element[1] == '-' && element[2] == '-'
			&& element[size - 2] == '-' && element[size - 1] == '-')
		{
			// update event
			reader->event = TB_XML_READER_EVENT_COMMENT;
		}
		// is element begin: <name>
		else
		{
			// update event
			reader->event = TB_XML_READER_EVENT_ELEMENT_BEG;
		}

		//TB_READER_DBG("element: %s", element);
	}
	// is text: <> text </>
	else 
	{
		// parse text: <> ... <>
		tb_char_t const* text = tb_xml_reader_parse_text(reader);
		if (!text) goto end;

		// update event
		reader->event = TB_XML_READER_EVENT_CHARACTERS;

		//TB_READER_DBG("text: %s", text);
	}

end:
	return reader->event;
}


/* /////////////////////////////////////////////////////////
 * interfaces: getter
 */
tb_size_t tb_xml_reader_get_event(tb_xml_reader_t* reader)
{
	if (reader) return reader->event;
	else return TB_XML_READER_EVENT_NULL;
}
tb_char_t const* tb_xml_reader_get_version(tb_xml_reader_t* reader)
{
	if (reader) return tb_string_c_string(&reader->version);
	else return TB_NULL;
}
tb_char_t const* tb_xml_reader_get_encoding(tb_xml_reader_t* reader)
{
	if (reader) return tb_string_c_string(&reader->encoding);
	else return TB_NULL;
}
tb_char_t const* tb_xml_reader_get_comment_text(tb_xml_reader_t* reader)
{
	TB_ASSERT(reader);
	if (!reader) return TB_NULL;

	// check event
	TB_ASSERT(reader->event == TB_XML_READER_EVENT_COMMENT);
	if (reader->event != TB_XML_READER_EVENT_COMMENT) return TB_NULL;

	// parse comment
	tb_char_t const* 	p = tb_string_c_string(&reader->element);
	tb_size_t 			n = tb_string_size(&reader->element);
	if (!p || n < 6) return TB_NULL;

	return tb_string_assign_c_string_with_size(&reader->text, p + 3, n - 5);
}
tb_char_t const* tb_xml_reader_get_characters_text(tb_xml_reader_t* reader)
{
	TB_ASSERT(reader);
	if (!reader) return TB_NULL;

	// check event
	TB_ASSERT(reader->event == TB_XML_READER_EVENT_CHARACTERS);
	if (reader->event != TB_XML_READER_EVENT_CHARACTERS) return TB_NULL;

	return tb_string_c_string(&reader->text);
}
tb_char_t const* tb_xml_reader_get_element_name(tb_xml_reader_t* reader)
{
	TB_ASSERT(reader);
	TB_ASSERT(TB_FALSE == tb_string_is_null(&reader->element));

	// check reader
	if (!reader) return TB_NULL;

	// check event
	TB_ASSERT( 	reader->event == TB_XML_READER_EVENT_ELEMENT_BEG
			|| 	reader->event == TB_XML_READER_EVENT_ELEMENT_END);

	if ( 	reader->event != TB_XML_READER_EVENT_ELEMENT_BEG 
		&& 	reader->event != TB_XML_READER_EVENT_ELEMENT_END)
		return TB_NULL;

	// parse the element name
	tb_char_t const* element = tb_string_c_string(&reader->element);
	if (element[0] == '/') return tb_string_assign_c_string_with_size_by_ref(&reader->name, element + 1, tb_string_size(&reader->element) - 1);
	else
	{
		// find the end position of the element
		tb_char_t const* p = element;
		tb_char_t const* e = p + tb_string_size(&reader->element);
		while (p < e && *p && !isspace(*p)) p++;

		if (p > element) return tb_string_assign_c_string_with_size(&reader->name, element, p - element);
	}
	return TB_NULL;
}
tb_size_t tb_xml_reader_get_attribute_count(tb_xml_reader_t* reader)
{
	return 0;
}
tb_char_t const* tb_xml_reader_get_attribute_name(tb_xml_reader_t* reader, tb_int_t index)
{
	return TB_NULL;
}
tb_char_t const* tb_xml_reader_get_attribute_value(tb_xml_reader_t* reader, tb_int_t index)
{
	return TB_NULL;
}
