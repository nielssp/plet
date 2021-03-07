/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#ifndef HTML_H
#define HTML_H

#include "strings.h"
#include "value.h"

void import_html(Env *env);

Value html_parse(String *html, Env *env);
void html_text_content(Value node, StringBuffer *buffer, Env *env);
Value html_find_tag(Value tag_name, Value node, Env *env);

#endif
