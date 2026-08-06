#pragma once

#include <memory>
#include <string>
#include <vector>
#include <variant>
#include <unordered_map>

#include "LOICollectionA/frontend/ASTVisitor.h"
#include "LOICollectionA/frontend/DiagnosticEngine.h"

namespace LOICollection::frontend {
    struct Object;
    using ObjectRef = std::shared_ptr<Object>;
    struct FunctionRef;
    using FunctionRefPtr = std::shared_ptr<FunctionRef>;

    enum class TypeKind {
        Unknown,
        Int,
        Float,
        String,
        Bool,
        Object,
        Function,
        Void
    };

    struct TypeInfo {
        TypeKind kind = TypeKind::Unknown;
        std::string className;

        bool operator==(const TypeInfo&) const = default;
    };

    struct ASTNode {
        enum class Type { 
            Value, Compare, Logical, If, Template, Expr,
            Arithmetic, Unary, Function, Macro, Variable, Assignment,
            Class, Return, New, MemberAccess, MethodCall, This,
            Super, SuperCall, InstanceOf, FunctionDef, FuncCall, Lambda
        };
        [[nodiscard]] virtual Type getType() const = 0;
        
        virtual ~ASTNode() = default;

        virtual void accept(ASTVisitor& visitor) = 0;
    };

    struct ExprNode : ASTNode {
        [[nodiscard]] Type getType() const override {
            return Type::Expr;
        }
    };

    struct ValueNode : ExprNode {
        using ValueType = std::variant<int, float, std::string, bool, ObjectRef, FunctionRefPtr>;

        ValueType value;
        
        explicit ValueNode(int v) : value(v) {}
        explicit ValueNode(float v) : value(v) {}
        explicit ValueNode(const std::string& v) : value(v) {}
        explicit ValueNode(bool v) : value(v) {}

        [[nodiscard]] Type getType() const override {
            return Type::Value;
        }

        void accept(ASTVisitor& visitor) override {
            visitor.visit(*this);
        }
    };

    struct VariableNode : ExprNode {
        SourceLocation loc;
        std::string name;

        VariableNode(SourceLocation location, std::string n) : loc(location), name(std::move(n)) {}

        [[nodiscard]] Type getType() const override { return Type::Variable; }

        void accept(ASTVisitor& visitor) override {
            visitor.visit(*this);
        }
    };

    struct AssignmentNode : ExprNode {
        SourceLocation loc;
        std::unique_ptr<ExprNode> target;
        std::unique_ptr<ExprNode> value;

        AssignmentNode(SourceLocation location, auto&& t, auto&& val)
            : loc(location),
              target(std::forward<decltype(t)>(t)),
              value(std::forward<decltype(val)>(val)) {}

        [[nodiscard]] Type getType() const override { return Type::Assignment; }

        void accept(ASTVisitor& visitor) override {
            visitor.visit(*this);
        }
    };

    struct IfNode : ExprNode {
        std::unique_ptr<ExprNode> condition;
        std::unique_ptr<ASTNode> trueBranch;
        std::unique_ptr<ASTNode> falseBranch;
        
        IfNode(auto&& c, auto&& t, auto&& f)
            : condition(std::forward<decltype(c)>(c)), 
              trueBranch(std::forward<decltype(t)>(t)), 
              falseBranch(std::forward<decltype(f)>(f)) {}
        
        [[nodiscard]] Type getType() const override {
            return Type::If;
        }

        void accept(ASTVisitor& visitor) override {
            visitor.visit(*this);
        }
    };

    struct CompareNode : ExprNode {
        std::unique_ptr<ExprNode> left;
        std::unique_ptr<ExprNode> right;
        std::string op;
        
        CompareNode(auto&& l, auto&& r, std::string o)
            : left(std::forward<decltype(l)>(l)),
              right(std::forward<decltype(r)>(r)),
              op(std::move(o)) {}
        
        [[nodiscard]] Type getType() const override {
            return Type::Compare;
        }

        void accept(ASTVisitor& visitor) override {
            visitor.visit(*this);
        }
    };

    struct LogicalNode : ExprNode {
        std::unique_ptr<ExprNode> left;
        std::unique_ptr<ExprNode> right;
        std::string op;
        
        LogicalNode(auto&& l, auto&& r, std::string o) 
            : left(std::forward<decltype(l)>(l)),
              right(std::forward<decltype(r)>(r)),
              op(std::move(o)) {}
        
        [[nodiscard]] Type getType() const override {
            return Type::Logical;
        }

        void accept(ASTVisitor& visitor) override {
            visitor.visit(*this);
        }
    };

    struct TemplateNode : ASTNode {
        std::vector<std::unique_ptr<ASTNode>> parts;

        [[nodiscard]] Type getType() const override {
            return Type::Template;
        }

        void addPart(auto&& part) {
            parts.emplace_back(std::forward<decltype(part)>(part));
        }

        void accept(ASTVisitor& visitor) override {
            visitor.visit(*this);
        }
    };

    struct FunctionNode : ExprNode {
        std::unique_ptr<TemplateNode> args; 
        std::string namespaces;
        std::string name;

        FunctionNode(auto&& a, std::string ns, std::string n)
            : args(std::forward<decltype(a)>(a)),
              namespaces(std::move(ns)),
              name(std::move(n)) {}
        
        [[nodiscard]] Type getType() const override {
            return Type::Function;
        }

        void accept(ASTVisitor& visitor) override {
            visitor.visit(*this);
        }
    };

    struct MacroNode : ExprNode {
        std::unique_ptr<TemplateNode> args;
        std::string name;

        MacroNode(auto&& a, std::string n)
            : args(std::forward<decltype(a)>(a)),
              name(std::move(n)) {}
        
        [[nodiscard]] Type getType() const override {
            return Type::Macro;
        }

        void accept(ASTVisitor& visitor) override {
            visitor.visit(*this);
        }
    };

    struct ArithmeticNode : ExprNode {
        std::unique_ptr<ExprNode> left;
        std::unique_ptr<ExprNode> right;
        std::string op;
        
        ArithmeticNode(auto&& l, auto&& r, std::string o)
            : left(std::forward<decltype(l)>(l)),
              right(std::forward<decltype(r)>(r)),
              op(std::move(o)) {}
        
        [[nodiscard]] Type getType() const override {
            return Type::Arithmetic;
        }

        void accept(ASTVisitor& visitor) override {
            visitor.visit(*this);
        }
    };

    struct UnaryNode : ExprNode {
        std::unique_ptr<ExprNode> operand;
        std::string op;
        
        UnaryNode(auto&& expr, std::string o)
            : operand(std::forward<decltype(expr)>(expr)),
              op(std::move(o)) {}
        
        [[nodiscard]] Type getType() const override {
            return Type::Unary;
        }

        void accept(ASTVisitor& visitor) override {
            visitor.visit(*this);
        }
    };

    struct ClassMember {
        SourceLocation loc;
        std::string name;
        bool isPrivate = false;
        std::unique_ptr<ExprNode> defaultExpr;
        TypeInfo type;
        bool hasDefault = false;
    };

    struct MethodParam {
        std::string name;
        std::string typeName;
        bool hasType = false;
    };

    struct MethodDecl {
        SourceLocation loc;
        std::string name;
        std::vector<MethodParam> params;
        std::string returnTypeName;
        bool hasReturnType = false;
        bool isConstructor = false;
        bool isPrivate = false;
        std::unique_ptr<ASTNode> body;

        std::vector<TypeInfo> paramTypes;
        TypeInfo returnType;
        bool hasReturnStatement = false;
        bool hasSuperCall = false;
    };

    struct ClassNode : ASTNode {
        SourceLocation loc;
        std::string name;
        std::string baseClassName;
        std::vector<ClassMember> members;
        std::vector<MethodDecl> methods;
        int constructorIndex = -1;

        ClassNode(SourceLocation location, std::string n)
            : loc(location), name(std::move(n)) {}

        [[nodiscard]] Type getType() const override { return Type::Class; }

        void accept(ASTVisitor& visitor) override {
            visitor.visit(*this);
        }
    };

    struct ReturnNode : ASTNode {
        SourceLocation loc;
        std::unique_ptr<ExprNode> value;

        ReturnNode(SourceLocation location, auto&& val)
            : loc(location), value(std::forward<decltype(val)>(val)) {}

        [[nodiscard]] Type getType() const override { return Type::Return; }

        void accept(ASTVisitor& visitor) override {
            visitor.visit(*this);
        }
    };

    struct NewNode : ExprNode {
        SourceLocation loc;
        std::string className;
        std::unique_ptr<TemplateNode> args;

        NewNode(SourceLocation location, std::string name, auto&& a)
            : loc(location), className(std::move(name)), args(std::forward<decltype(a)>(a)) {}

        [[nodiscard]] Type getType() const override { return Type::New; }

        void accept(ASTVisitor& visitor) override {
            visitor.visit(*this);
        }
    };

    struct MemberAccessNode : ExprNode {
        SourceLocation loc;
        std::unique_ptr<ExprNode> target;
        std::string memberName;

        MemberAccessNode(SourceLocation location, auto&& t, std::string member)
            : loc(location), target(std::forward<decltype(t)>(t)), memberName(std::move(member)) {}

        [[nodiscard]] Type getType() const override { return Type::MemberAccess; }

        void accept(ASTVisitor& visitor) override {
            visitor.visit(*this);
        }
    };

    struct MethodCallNode : ExprNode {
        SourceLocation loc;
        std::unique_ptr<ExprNode> target;
        std::string methodName;
        std::unique_ptr<TemplateNode> args;

        std::string className;
        int methodOrdinal = -1;

        MethodCallNode(SourceLocation location, auto&& t, std::string name, auto&& a)
            : loc(location),
              target(std::forward<decltype(t)>(t)),
              methodName(std::move(name)),
              args(std::forward<decltype(a)>(a)) {}

        [[nodiscard]] Type getType() const override { return Type::MethodCall; }

        void accept(ASTVisitor& visitor) override {
            visitor.visit(*this);
        }
    };

    struct ThisNode : ExprNode {
        SourceLocation loc;

        explicit ThisNode(SourceLocation location) : loc(location) {}

        [[nodiscard]] Type getType() const override { return Type::This; }

        void accept(ASTVisitor& visitor) override {
            visitor.visit(*this);
        }
    };

    struct SuperNode : ExprNode {
        SourceLocation loc;

        explicit SuperNode(SourceLocation location) : loc(location) {}

        [[nodiscard]] Type getType() const override { return Type::Super; }

        void accept(ASTVisitor& visitor) override {
            visitor.visit(*this);
        }
    };

    struct SuperCallNode : ExprNode {
        SourceLocation loc;
        std::unique_ptr<TemplateNode> args;

        std::string className;
        int constructorIndex = -1;

        SuperCallNode(SourceLocation location, auto&& a)
            : loc(location), args(std::forward<decltype(a)>(a)) {}

        [[nodiscard]] Type getType() const override { return Type::SuperCall; }

        void accept(ASTVisitor& visitor) override {
            visitor.visit(*this);
        }
    };

    struct InstanceOfNode : ExprNode {
        SourceLocation loc;
        std::unique_ptr<ExprNode> target;
        std::string className;

        InstanceOfNode(SourceLocation location, auto&& t, std::string name)
            : loc(location), target(std::forward<decltype(t)>(t)), className(std::move(name)) {}

        [[nodiscard]] Type getType() const override { return Type::InstanceOf; }

        void accept(ASTVisitor& visitor) override {
            visitor.visit(*this);
        }
    };

    struct FunctionDefNode : ASTNode {
        SourceLocation loc;
        std::string name;
        MethodDecl decl;

        FunctionDefNode(SourceLocation location, std::string n)
            : loc(location), name(std::move(n)) {}

        [[nodiscard]] Type getType() const override { return Type::FunctionDef; }

        void accept(ASTVisitor& visitor) override {
            visitor.visit(*this);
        }
    };

    struct LambdaNode : ExprNode {
        SourceLocation loc;
        MethodDecl decl;

        explicit LambdaNode(SourceLocation location) : loc(location) {}

        [[nodiscard]] Type getType() const override { return Type::Lambda; }

        void accept(ASTVisitor& visitor) override {
            visitor.visit(*this);
        }
    };

    struct FuncCallNode : ExprNode {
        SourceLocation loc;
        std::string name;
        std::unique_ptr<TemplateNode> args;

        std::string resolvedName;
        int functionOrdinal = -1;
        bool isCallable = false;

        FuncCallNode(SourceLocation location, std::string n, auto&& a)
            : loc(location), name(std::move(n)), args(std::forward<decltype(a)>(a)) {}

        [[nodiscard]] Type getType() const override { return Type::FuncCall; }

        void accept(ASTVisitor& visitor) override {
            visitor.visit(*this);
        }
    };

    struct Object {
        std::string className;
        int classIndex = -1;
        std::unordered_map<std::string, ValueNode::ValueType> fields;
    };

    struct FunctionRef {
        int bodyIndex = -1;
        int argCount = 0;
        std::vector<std::string> paramNames;
        bool hasThis = false;
        ObjectRef thisObj;
        std::unordered_map<std::string, ValueNode::ValueType> captures;
    };
}
