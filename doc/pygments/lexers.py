from pygments.lexer import *
from pygments.token import *
from pygments.lexers.html import HtmlLexer

class TscLexer(RegexLexer):
    name = 'TSC'
    aliases = ['tsc']
    filenames = ['*.tss']

    tokens = {
        'root': [
            include('command'),
        ],
        'command': [
            (r'\{\#.*?\#\}', Comment.Multiline),
            (r'#.*?\n', Comment.Single),
            (r'(if|then|else|for|in|switch|case|default|end|and|or|not|do|export|return|break|continue)\b', Keyword),
            (r'(true|false|nil)\b', Keyword.Constant),
            (r"'(\\\\|\\[^\\]|[^'\\])*'", String.Single),
            (r"\b[0-9]+\b", Number),
            (r"\b[a-zA-Z_][a-zA-Z0-9_]*\b", Name),
            (r'\+|-|\*|/|<=|>=|==|!=|<|>|=|\?', Operator),
            (r'[\.,:|]', Punctuation),
            (r'\[', Punctuation, 'array'),
            (r'\(', Punctuation, 'expression'),
            (r'\{', Punctuation, 'statements'),
            (r'\}', Comment.Preproc, 'template'),
            (r'"', String, 'string'),
            ('\s+', Whitespace),
        ],
        'template': [
            (r'[^{]+', String),
            (r'\{\#.*?\#\}', Comment.Multiline),
            (r'\{', Comment.Preproc, '#pop'),
        ],
        'string': [
            (r'[^"{]+', String),
            (r'\{\#.*?\#\}', Comment.Multiline),
            (r'\{', Comment.Preproc, 'statements'),
            ('"', String, '#pop'),
        ],
        'statements': [
            (r'\}', Comment.Preproc, '#pop'),
            include('command'),
        ],
        'array': [
            (r'\]', Punctuation, '#pop'),
            include('command'),
        ],
        'expression': [
            (r'\)', Punctuation, '#pop'),
            include('command'),
        ],
    }

class TxtTscLexer(TscLexer):
    name = 'TXT+TSC'
    aliases = ['txt+tsc']
    filenames = ['*.txt.tss']

    tokens = {
        'root': [
            (r'[^{]+', Other),
            (r'\{\#.*?\#\}', Comment.Multiline),
            (r'\{', Comment.Preproc, 'statements'),
        ],
    }

class HtmlTscLexer(DelegatingLexer):
    name = 'HTML+TSC'
    aliases = ['html+tsc']
    filenames = ['*.html.tss']

    def __init__(self, **options):
        super().__init__(HtmlLexer, TxtTscLexer, **options)
