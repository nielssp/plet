/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "html.h"

#include "build.h"
#include "strings.h"
#include "template.h"

#include <string.h>

#ifdef WITH_GUMBO
#include <gumbo.h>
#endif

static void html_encode_byte(StringBuffer *buffer, uint8_t byte) {
  switch (byte) {
    case '&':
      string_buffer_printf(buffer, "&amp;");
      break;
    case '"':
      string_buffer_printf(buffer, "&quot;");
      break;
    case '\'':
      string_buffer_printf(buffer, "&#39;");
      break;
    case '<':
      string_buffer_printf(buffer, "&lt;");
      break;
    case '>':
      string_buffer_printf(buffer, "&gt;");
      break;
    default:
      string_buffer_put(buffer, byte);
      break;
  }
}

static Value h(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value value = args->values[0];
  StringBuffer buffer = create_string_buffer(0, env->arena);
  switch (value.type) {
    case V_SYMBOL:
      while (*value.symbol_value) {
        html_encode_byte(&buffer, *value.symbol_value);
        value.symbol_value++;
      }
      break;
    case V_STRING:
      for (size_t i = 0; i < value.string_value->size; i++) {
        html_encode_byte(&buffer, value.string_value->bytes[i]);
      }
      break;
    default:
      string_buffer_append_value(&buffer, value);
      break;
  }
  return finalize_string_buffer(buffer);
}

static Value href(const Tuple *args, Env *env) {
  check_args_between(0, 2, args, env);
  Value path;
  Value class = create_string(NULL, 0, env->arena);
  if (args->size > 0) {
    path = args->values[0];
    if (path.type != V_STRING) {
      arg_type_error(0, V_STRING, args, env);
      return nil_value;
    }
    if (args->size > 1) {
      class = args->values[1];
      if (class.type != V_STRING) {
        arg_type_error(1, V_STRING, args, env);
        return nil_value;
      }
    }
  } else if (!env_get(get_symbol("PATH", env->symbol_map), &path, env) || path.type != V_STRING) {
    env_error(env, -1, "PATH is not set or not a string");
    return nil_value;
  }
  if (string_equals("index.html", path.string_value)) {
    path = copy_c_string("", env->arena);
  } else if (string_ends_with("/index.html", path.string_value)) {
    path = create_string(path.string_value->bytes, path.string_value->size - 11, env->arena);
  }
  if (path_is_current(path.string_value, env)) {
    if (class.string_value->size) {
      StringBuffer buffer = create_string_buffer(class.string_value->size + 8, env->arena);
      string_buffer_append(&buffer, class.string_value);
      string_buffer_printf(&buffer, " current");
      class = finalize_string_buffer(buffer);
    } else {
      class = copy_c_string("current", env->arena);
    }
  }
  Value root_path;
  if (env_get(get_symbol("ROOT_PATH", env->symbol_map), &root_path, env) && root_path.type == V_STRING) {
    path = combine_string_paths(root_path.string_value, path.string_value, env);
  }
  StringBuffer buffer = create_string_buffer(0, env->arena);
  string_buffer_printf(&buffer, " href=\"");
  for (size_t i = 0; i < path.string_value->size; i++) {
    html_encode_byte(&buffer, path.string_value->bytes[i]);
  }
  string_buffer_printf(&buffer, "\"");
  if (class.string_value->size) {
    string_buffer_printf(&buffer, " class=\"");
    for (size_t i = 0; i < class.string_value->size; i++) {
      html_encode_byte(&buffer, class.string_value->bytes[i]);
    }
    string_buffer_printf(&buffer, "\"");
  }
  return finalize_string_buffer(buffer);
}

static void html_to_string(Value node, StringBuffer *buffer, Env *env) {
  if (node.type == V_OBJECT) {
    Value tag = nil_value;
    object_get(node.object_value, create_symbol(get_symbol("tag", env->symbol_map)), &tag);
    if (tag.type == V_SYMBOL) {
      string_buffer_put(buffer, '<');
      string_buffer_printf(buffer, "%s", tag.symbol_value);
      Value attributes;
      if (object_get(node.object_value, create_symbol(get_symbol("attributes", env->symbol_map)),
            &attributes) && attributes.type == V_OBJECT) {
        ObjectIterator it = iterate_object(attributes.object_value);
        Value key, value;
        while (object_iterator_next(&it, &key, &value)) {
          if (key.type == V_SYMBOL && value.type == V_STRING) {
            string_buffer_put(buffer, ' ');
            string_buffer_printf(buffer, "%s", key.symbol_value);
            if (value.string_value->size) {
              string_buffer_put(buffer, '=');
              string_buffer_put(buffer, '"');
              for (size_t i = 0; i < value.string_value->size; i++) {
                html_encode_byte(buffer, value.string_value->bytes[i]);
              }
              string_buffer_put(buffer, '"');
            }
          }
        }
      }
      string_buffer_put(buffer, '>');
    }
    Value children;
    if (object_get(node.object_value, create_symbol(get_symbol("children", env->symbol_map)), &children)
        && children.type == V_ARRAY) {
      for (size_t i = 0; i < children.array_value->size; i++) {
        html_to_string(children.array_value->cells[i], buffer, env);
      }
    }
    Value self_closing = nil_value;
    object_get(node.object_value, create_symbol(get_symbol("self_closing", env->symbol_map)),
        &self_closing);
    if (tag.type == V_SYMBOL && !is_truthy(self_closing)) {
      string_buffer_put(buffer, '<');
      string_buffer_put(buffer, '/');
      string_buffer_printf(buffer, "%s", tag.symbol_value);
      string_buffer_put(buffer, '>');
    }
  } else if (node.type == V_STRING) {
    for (size_t i = 0; i < node.string_value->size; i++) {
      html_encode_byte(buffer, node.string_value->bytes[i]);
    }
  }
}

static Value html(const Tuple *args, Env *env) {
  check_args(1, args, env);
  StringBuffer output = create_string_buffer(0, env->arena);
  html_to_string(args->values[0], &output, env);
  return finalize_string_buffer(output);
}

static Value no_title(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value src = args->values[0];
  Value title_tag = html_find_tag(get_symbol("h1", env->symbol_map), src);
  if (title_tag.type == V_OBJECT) {
    src = copy_value(src, env->arena);
    title_tag = html_find_tag(get_symbol("h1", env->symbol_map), src);
    if (title_tag.type == V_OBJECT) {
      html_remove_node(title_tag.object_value, src);
    }
  }
  return src;
}

typedef struct {
  int absolute;
  Path *src_root;
  Path *dist_root;
  Path *asset_root;
  Env *env;
} LinkArgs;

static int transform_link(Value node, const char *attribute_name, LinkArgs *args) {
  Value src = html_get_attribute(node, attribute_name);
  if (src.type != V_STRING) {
    return 0;
  }
  if (string_starts_with("tscasset:", src.string_value)) {
    Path *asset_path = create_path((char *) src.string_value->bytes + sizeof("tscasset:") - 1,
        src.string_value->size - (sizeof("tscasset:") - 1));
    Path *src_path = path_join(args->src_root, asset_path);
    Path *asset_web_path = path_join(args->asset_root, asset_path);
    Path *dist_path = path_join(args->dist_root, asset_web_path);
    copy_asset(src_path, dist_path);
    html_set_attribute(node, attribute_name, get_web_path(asset_web_path, args->absolute, args->env).string_value,
        args->env);
    delete_path(dist_path);
    delete_path(asset_web_path);
    delete_path(src_path);
    delete_path(asset_path);
  } else if (string_starts_with("tsclink:", src.string_value)) {
    Path *web_path = create_path((char *) src.string_value->bytes + sizeof("tsclink:") - 1,
        src.string_value->size - (sizeof("tsclink:") - 1));
    html_set_attribute(node, attribute_name, get_web_path(web_path, args->absolute, args->env).string_value,
        args->env);
    delete_path(web_path);
  }
  return 1;
}

static HtmlTransformation transform_links(Value node, void *context) {
  LinkArgs *args = context;
  if (!transform_link(node, "src", args)) {
    transform_link(node, "href", args);
  }
  return HTML_NO_ACTION;
}

static Value links_or_urls(Value src, int absolute, Env *env) {
  Path *src_root = get_src_root(env);
  if (src_root) {
    Path *dist_root = get_dist_root(env);
    if (dist_root) {
      Path *asset_root = create_path("assets", -1);
      LinkArgs context = {absolute, src_root, dist_root, asset_root, env};
      src = html_transform(src, transform_links, &context);
      delete_path(asset_root);
      delete_path(dist_root);
    } else {
      env_error(env, -1, "DIST_ROOT missing or not a string");
    }
    delete_path(src_root);
  } else {
    env_error(env, -1, "SRC_ROOT missing or not a string");
  }
  return src;
}

static Value links(const Tuple *args, Env *env) {
  check_args(0, args, env);
  Value src = args->values[0];
  return links_or_urls(src, 0, env);
}

static Value urls(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value src = args->values[0];
  return links_or_urls(src, 1, env);
}

#ifdef WITH_GUMBO
static Value parse_html(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value src = args->values[0];
  if (src.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
    return nil_value;
  }
  return html_parse(src.string_value, env);
}
#endif

void import_html(Env *env) {
  env_def_fn("h", h, env);
  env_def_fn("href", href, env);
  env_def_fn("html", html, env);
  env_def_fn("no_title", no_title, env);
  env_def_fn("links", links, env);
  env_def_fn("urls", urls, env);
#ifdef WITH_GUMBO
  env_def_fn("parse_html", parse_html, env);
#endif
}

#ifdef WITH_GUMBO

static Value convert_gumbo_node(GumboNode *node, Env *env) {
  switch (node->type) {
    case GUMBO_NODE_DOCUMENT: {
      Value obj = create_object(2, env->arena);
      object_def(obj.object_value, "type", create_symbol(get_symbol("document", env->symbol_map)), env);
      Value children = create_array(node->v.element.children.length, env->arena);
      for (unsigned int i = 0; i < node->v.element.children.length; i++) {
        Value child = convert_gumbo_node(node->v.element.children.data[i], env);
        if (child.type != V_NIL) {
          array_push(children.array_value, child, env->arena);
        }
      }
      object_def(obj.object_value, "children", children, env);
      return obj;
    }
    case GUMBO_NODE_ELEMENT: {
      Value obj = create_object(4, env->arena);
      object_def(obj.object_value, "type", create_symbol(get_symbol("element", env->symbol_map)), env);
      object_def(obj.object_value, "tag",
          create_symbol(get_symbol(gumbo_normalized_tagname(node->v.element.tag), env->symbol_map)),
          env);
      Value attributes = create_object(node->v.element.attributes.length, env->arena);
      for (unsigned int i = 0; i < node->v.element.attributes.length; i++) {
        GumboAttribute *attribute = node->v.element.attributes.data[i];
        object_put(attributes.object_value, create_symbol(get_symbol(attribute->name, env->symbol_map)),
            copy_c_string(attribute->value, env->arena), env->arena);
      }
      object_def(obj.object_value, "attributes", attributes, env);
      Value children = create_array(node->v.element.children.length, env->arena);
      for (unsigned int i = 0; i < node->v.element.children.length; i++) {
        Value child = convert_gumbo_node(node->v.element.children.data[i], env);
        if (child.type != V_NIL) {
          array_push(children.array_value, child, env->arena);
        }
      }
      object_def(obj.object_value, "children", children, env);
      object_def(obj.object_value, "self_closing",
          node->v.element.original_end_tag.length == 0 ? true_value : nil_value, env);
      return obj;
    }
    case GUMBO_NODE_TEXT:
    case GUMBO_NODE_CDATA:
      return copy_c_string(node->v.text.text, env->arena);
    case GUMBO_NODE_COMMENT:
      return nil_value;
    case GUMBO_NODE_WHITESPACE:
      return copy_c_string(node->v.text.text, env->arena);
    case GUMBO_NODE_TEMPLATE:
    default:
      return nil_value;
  }
}

Value html_parse(String *html, Env *env) {
  GumboOptions options = kGumboDefaultOptions;
  options.fragment_context = GUMBO_TAG_DIV;
  GumboOutput *output = gumbo_parse_with_options(&options, (char *) html->bytes, html->size);
  Value root = convert_gumbo_node(output->root, env);
  gumbo_destroy_output(&options, output);
  if (root.type == V_OBJECT) {
      object_def(root.object_value, "type", create_symbol(get_symbol("fragment", env->symbol_map)), env);
      object_def(root.object_value, "tag", nil_value, env);
  }
  return root;
}

#else /* ifdef WITH_GUMBO */

Value html_parse(String *html, Env *env) {
  return nil_value;
}

#endif /* ifdef WITH_GUMBO */

void html_text_content(Value node, StringBuffer *buffer) {
  if (node.type == V_OBJECT) {
    Value children;
    if (object_get_symbol(node.object_value, "children", &children)) {
      if (children.type == V_ARRAY) {
        for (size_t i = 0; i < children.array_value->size; i++) {
          html_text_content(children.array_value->cells[i], buffer);
        }
      }
    }
  } else if (node.type == V_STRING) {
    string_buffer_append(buffer, node.string_value);
  }
}

Value html_find_tag(Symbol tag_name, Value node) {
  if (node.type == V_OBJECT) {
    Value node_tag;
    if (object_get_symbol(node.object_value, "tag", &node_tag)) {
      if (node_tag.type == V_SYMBOL && node_tag.symbol_value == tag_name) {
        return node;
      }
    }
    Value children;
    if (object_get_symbol(node.object_value, "children", &children)
        && children.type == V_ARRAY) {
      for (size_t i = 0; i < children.array_value->size; i++) {
        Value result = html_find_tag(tag_name, children.array_value->cells[i]);
        if (result.type != V_NIL) {
          return result;
        }
      }
    }
  }
  return nil_value;
}

int html_remove_node(Object *needle, Value haystack) {
  if (haystack.type == V_OBJECT) {
    if (haystack.object_value == needle) {
      return 1;
    }
    Value children;
    if (object_get_symbol(haystack.object_value, "children", &children)
        && children.type == V_ARRAY) {
      for (size_t i = 0; i < children.array_value->size; i++) {
        if (html_remove_node(needle, children.array_value->cells[i])) {
          array_remove(children.array_value, i);
          break;
        }
      }
    }
  }
  return 0;
}

static HtmlTransformation internal_html_transform(Value node, HtmlTransformation (*acceptor)(Value, void *),
    void *context);

static HtmlTransformation internal_html_transform(Value node, HtmlTransformation (*acceptor)(Value, void *),
    void *context) {
  HtmlTransformation transformation = acceptor(node, context);
  if (transformation.type != HT_NO_ACTION) {
    return transformation;
  }
  if (node.type == V_OBJECT) {
    Value children;
    if (object_get_symbol(node.object_value, "children", &children) && children.type == V_ARRAY) {
      for (size_t i = 0; i < children.array_value->size; i++) {
        HtmlTransformation child_ht = internal_html_transform(children.array_value->cells[i], acceptor,
            context);
        if (child_ht.type == HT_REMOVE) {
          array_remove(children.array_value, i);
          i--;
        } else if (child_ht.type == HT_REPLACE) {
          children.array_value->cells[i] = child_ht.replacement;
        }
      }
    }
  }
  return transformation;
}

Value html_transform(Value node, HtmlTransformation (*acceptor)(Value, void *), void *context) {
  HtmlTransformation transformation = internal_html_transform(node, acceptor, context);
  if (transformation.type == HT_REMOVE) {
    return nil_value;
  } else if (transformation.type == HT_REPLACE) {
    return transformation.replacement;
  } else {
    return node;
  }
}

int html_is_tag(Value node, const char *tag_name) {
  if (node.type == V_OBJECT) {
    Value node_tag;
    if (object_get_symbol(node.object_value, "tag", &node_tag)) {
      if (node_tag.type == V_SYMBOL && strcmp(node_tag.symbol_value, tag_name) == 0) {
        return 1;
      }
    }
  }
  return 0;
}

Value html_get_attribute(Value node, const char *attribute_name) {
  if (node.type == V_OBJECT) {
    Value attributes;
    if (object_get_symbol(node.object_value, "attributes", &attributes) && attributes.type == V_OBJECT) {
      Value attribute;
      if (object_get_symbol(attributes.object_value, attribute_name, &attribute)) {
        return attribute;
      }
    }
  }
  return nil_value;
}

void html_set_attribute(Value node, const char *attribute_name, String *string_value, Env *env) {
  if (node.type == V_OBJECT) {
    Value attributes;
    if (object_get_symbol(node.object_value, "attributes", &attributes) && attributes.type == V_OBJECT) {
      Value value = (Value) { .type = V_STRING, .string_value = string_value };
      object_def(attributes.object_value, attribute_name, value, env);
    }
  }
}
