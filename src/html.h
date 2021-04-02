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
void html_text_content(Value node, StringBuffer *buffer);
Value html_find_tag(Symbol tag_name, Value node);
int html_remove_node(Object *needle, Value haystack);

typedef enum {
  HT_NO_ACTION,
  HT_REMOVE,
  HT_REPLACE
} HtmlTransformationType;

typedef struct {
  HtmlTransformationType type;
  Value replacement;
} HtmlTransformation;

#define HTML_NO_ACTION ((HtmlTransformation) { .type = HT_NO_ACTION })
#define HTML_REMOVE ((HtmlTransformation) { .type = HT_REMOVE })
#define HTML_REPLACE(REPLACEMENT) ((HtmlTransformation) { .type = HT_REPLACE, .replacement = (REPLACEMENT) })

Value html_transform(Value node, HtmlTransformation (*acceptor)(Value, void *), void *context);

int html_is_tag(Value node, const char *tag_name);
Value html_create_element(const char *tag_name, int self_closing, Env *env);
void html_prepend_child(Value node, Value child, Arena *arena);
void html_append_child(Value node, Value child, Arena *arena);
Value html_get_attribute(Value node, const char *attribute_name);
void html_set_attribute(Value node, const char *attribute_name, String *value, Env *env);
void html_error(Value node, const Path *path, const char *format, ...);

#endif
