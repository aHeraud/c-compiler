#ifndef C_COMPILER_AST_H
#define C_COMPILER_AST_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "lexer.h"

typedef struct AstNode ast_node_t;
typedef struct AstNodeVector {
    ast_node_t** buffer;
    size_t size;
    size_t capacity;
} ast_node_vector_t;

typedef struct Identifier {
    char* name;
} identifier_t;

typedef struct Constant {
    enum {
        CONSTANT_INTEGER,
        CONSTANT_FLOATING,
        CONSTANT_CHARACTER,
    } type;
    union {
        int64_t integer;
        double floating;
        char character;
    };
} constant_t;

typedef struct PrimaryExpression {
    enum {
        PE_IDENTIFIER,
        PE_CONSTANT,
        PE_STRING_LITERAL,
        PE_EXPRESSION,
    } type;
    union {
        identifier_t identifier;
        constant_t constant;
        char* string_literal;
    };
} primary_expression_t;

typedef struct Declaration {
    ast_node_t* declaration_specifiers;
    ast_node_t* init_declarators;
} declaration_t;

typedef enum StorageClassSpecifier {
    STORAGE_CLASS_SPECIFIER_TYPEDEF,
    STORAGE_CLASS_SPECIFIER_EXTERN,
    STORAGE_CLASS_SPECIFIER_STATIC,
    STORAGE_CLASS_SPECIFIER_AUTO,
    STORAGE_CLASS_SPECIFIER_REGISTER,
} storage_class_specifier_t;

static const char* storage_class_specifier_names[] = {
        [STORAGE_CLASS_SPECIFIER_TYPEDEF] = "typedef",
        [STORAGE_CLASS_SPECIFIER_EXTERN] = "extern",
        [STORAGE_CLASS_SPECIFIER_STATIC] = "static",
        [STORAGE_CLASS_SPECIFIER_AUTO] = "auto",
        [STORAGE_CLASS_SPECIFIER_REGISTER] = "register",
};

// TODO: struct, union, enum, typedef
typedef enum TypeSpecifier {
    TYPE_SPECIFIER_VOID,
    TYPE_SPECIFIER_CHAR,
    TYPE_SPECIFIER_SHORT,
    TYPE_SPECIFIER_INT,
    TYPE_SPECIFIER_LONG,
    TYPE_SPECIFIER_FLOAT,
    TYPE_SPECIFIER_DOUBLE,
    TYPE_SPECIFIER_SIGNED,
    TYPE_SPECIFIER_UNSIGNED,
    TYPE_SPECIFIER_BOOL,
    TYPE_SPECIFIER_COMPLEX,
    TYPE_SPECIFIER_STRUCT,
    TYPE_SPECIFIER_UNION,
    TYPE_SPECIFIER_ENUM,
    TYPE_SPECIFIER_TYPEDEF_NAME,
} type_specifier_t;

static const char* type_specifier_names[] = {
        [TYPE_SPECIFIER_VOID] = "void",
        [TYPE_SPECIFIER_CHAR] = "char",
        [TYPE_SPECIFIER_SHORT] = "short",
        [TYPE_SPECIFIER_INT] = "int",
        [TYPE_SPECIFIER_LONG] = "long",
        [TYPE_SPECIFIER_FLOAT] = "float",
        [TYPE_SPECIFIER_DOUBLE] = "double",
        [TYPE_SPECIFIER_SIGNED] = "signed",
        [TYPE_SPECIFIER_UNSIGNED] = "unsigned",
        [TYPE_SPECIFIER_BOOL] = "bool",
        [TYPE_SPECIFIER_COMPLEX] = "complex",
        [TYPE_SPECIFIER_STRUCT] = "struct",
        [TYPE_SPECIFIER_UNION] = "union",
        [TYPE_SPECIFIER_ENUM] = "enum",
        [TYPE_SPECIFIER_TYPEDEF_NAME] = "typedef",
};

typedef enum TypeQualifier {
    TYPE_QUALIFIER_CONST,
    TYPE_QUALIFIER_RESTRICT,
    TYPE_QUALIFIER_VOLATILE,
} type_qualifier_t;

static const char* type_qualifier_names[] = {
        [TYPE_QUALIFIER_CONST] = "const",
        [TYPE_QUALIFIER_RESTRICT] = "restrict",
        [TYPE_QUALIFIER_VOLATILE] = "volatile",
};

typedef enum FunctionSpecifier {
    FUNCTION_SPECIFIER_INLINE,
} function_specifier_t;

static const char* function_specifier_names[] = {
        [FUNCTION_SPECIFIER_INLINE] = "inline",
};

typedef struct InitDeclarator {
    ast_node_t* declarator;
    ast_node_t* initializer;
} init_declarator_t;

typedef struct AbstractDeclarator {
    ast_node_t* pointer;
    ast_node_t* direct_abstract_declarator;
} abstract_declarator_t;

typedef struct DirectAbstractDeclarator {
    enum DIRECT_ABSTRACT_DECLARATOR_TYPE {
        DIRECT_ABSTRACT_DECL_ABSTRACT,
        DIRECT_ABSTRACT_DECL_ARRAY,
        DIRECT_ABSTRACT_DECL_FUNCTION,
    } type;
    union {
        abstract_declarator_t abstract;
        struct {
            ast_node_t* type_qualifier_list;
            ast_node_t* assignment_expression;
            bool _static;
        } array;
        struct {
            ast_node_t* param_type_list;
        } function;
    };
    ast_node_t* next;
    ast_node_t* prev;
} direct_abstract_declarator_t;

typedef struct Declarator {
    ast_node_t* pointer;
    ast_node_t* direct_declarator;
} declarator_t;

typedef struct DirectDeclarator {
    enum DECL_TYPE {
        DECL_IDENTIFIER,
        DECL_ARRAY,
        DECL_FUNCTION
    } type;
    union {
        identifier_t identifier;
        struct {
            ast_node_t* type_qualifier_list;
            ast_node_t* assignment_expression;
            bool _static;
            bool pointer;
        } array;
        struct {
            ast_node_t* param_type_or_ident_list;
        } function;
    };
    ast_node_t* next;
    ast_node_t* prev;
} direct_declarator_t;

typedef struct Initializer {
    enum INITIALIZER_TYPE {
        INITIALIZER_EXPRESSION,
        INITIALIZER_LIST, // struct or array initializer
    } type;
    union {
        ast_node_t* expression;
        ast_node_t* initializer_list;
    };
} initializer_t;

typedef struct FunctionDefinition {
    ast_node_t* declaration_specifiers;
    ast_node_t* declarator;
    ast_node_t* declaration_list;
    ast_node_t* compound_statement;
} function_definition_t;

typedef struct ParameterDeclaration {
    ast_node_t* declaration_specifiers;
    ast_node_t* declarator; // declarator, or optional abstract declarator
} parameter_declaration_t;

typedef struct ParameterTypeList {
    ast_node_vector_t parameter_list;
    bool variadic;
} parameter_type_list_t;

typedef struct Pointer {
    ast_node_t* type_qualifier_list;
    ast_node_t* next_pointer;
} pointer_t;

typedef struct TranslationUnit {
    ast_node_vector_t external_declarations;
} translation_unit_t;

typedef struct CompoundStatement {
    ast_node_vector_t block_items;
} compound_statement_t;

typedef enum JumpStatementKind {
    JMP_GOTO,
    JMP_CONTINUE,
    JMP_BREAK,
    JMP_RETURN,
} jump_statement_kind_t;

typedef struct JumpStatement {
    jump_statement_kind_t type;
    union {
        struct {
            identifier_t identifier;
        } _goto;
        struct {
            ast_node_t* expression;
        } _return;
    };
} jump_statement_t;

typedef enum AstNodeKind {
    AST_ABSTRACT_DECLARATOR,
    AST_EXPRESSION,
    AST_PRIMARY_EXPRESSION,
    AST_DECLARATION,
    AST_DECLARATION_SPECIFIERS,
    AST_STORAGE_CLASS_SPECIFIER,
    AST_TYPE_SPECIFIER,
    AST_TYPE_QUALIFIER,
    AST_FUNCTION_SPECIFIER,
    AST_INIT_DECLARATOR_LIST,
    AST_INIT_DECLARATOR,
    AST_DECLARATOR,
    AST_DIRECT_ABSTRACT_DECLARATOR,
    AST_DIRECT_DECLARATOR,
    AST_INITIALIZER,
    AST_FUNCTION_DEFINITION,
    AST_TRANSLATION_UNIT,
    AST_COMPOUND_STATEMENT,
    AST_JUMP_STATEMENT,
    AST_PARAMETER_TYPE_LIST,
    AST_PARAMETER_DECLARATION,
    AST_POINTER,
} ast_node_kind_t;

typedef struct AstNode {
    ast_node_kind_t type;
    source_position_t position;
    // There's probably a better way to do this, but I'm not sure what it is
    union {
        abstract_declarator_t abstract_declarator;
        direct_abstract_declarator_t direct_abstract_declarator;
        primary_expression_t primary_expression;
        declaration_t declaration;
        ast_node_vector_t declaration_specifiers;
        storage_class_specifier_t storage_class_specifier;
        type_specifier_t type_specifier;
        type_qualifier_t type_qualifier;
        function_specifier_t function_specifier;
        ast_node_vector_t init_declarator_list;
        init_declarator_t init_declarator;
        declarator_t declarator;
        direct_declarator_t direct_declarator;
        initializer_t initializer;
        function_definition_t function_definition;
        translation_unit_t translation_unit;
        compound_statement_t compound_statement;
        jump_statement_t jump_statement;
        parameter_declaration_t parameter_declaration;
        parameter_type_list_t parameter_type_list;
        pointer_t pointer;
    };
} ast_node_t;

#endif //C_COMPILER_AST_H
