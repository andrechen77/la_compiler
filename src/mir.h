#pragma once

#include "std_alias.h"
#include <variant>
#include <string>

// The MIR, or "mid-level intermediate representation", describes the imperative
// instructions of the LA program at a type-aware level. Each function is
// a control flow graph of BasicBlocks which contain lists of elementary
// type-aware instructions as well as transitions to other BasicBlocks. It is
// also meant to closely reflect CS 322's IR language.
namespace mir {
	using namespace std_alias;

	struct Type {
		struct VoidType {};
		struct ArrayType { int num_dimensions; };
		struct TupleType {};
		struct CodeType {};
		using Variant = std::variant<VoidType, ArrayType, TupleType, CodeType>;
		Variant type;

		std::string to_string() const;
	};

	// any function-local location in memory, including user-defined local
	// variables as well as compiler-defined temporaries
	struct LocalVar {
		std::string user_given_name; // empty means anonymous
		Type type;

		LocalVar(std::string user_given_name, Type type) :
			user_given_name { mv(user_given_name) }, type { type }
		{}

		std::string to_string() const;
		std::string get_unambiguous_name() const;
		std::string get_declaration() const;
	};

	// a value that can be used as the right-hand side of an
	// InstructionAssignment
	// closely resembles hir::Expr
	struct Rvalue {
		virtual std::string to_string() const = 0;
	};

	struct Operand : Rvalue {};

	// a "place" in memory, which can be assigned to as the left-hand side of
	// an InstructionAssignment.
	// closely resembles hir::IndexingExpr but is more limited in the allowable'
	// target expressions
	struct Place : Operand {
		LocalVar *target;
		Vec<Uptr<Operand>> indices;

		Place(LocalVar *target, Vec<Uptr<Operand>> indices) :
			target { target }, indices { mv(indices) }
		{}

		std::string to_string() const override;
	};

	struct Int64Constant : Operand {
		int64_t value;

		Int64Constant(int64_t value) : value { value } {}

		std::string to_string() const override;
	};

	struct FunctionDef;

	struct CodeConstant : Operand {
		FunctionDef *value;

		CodeConstant(FunctionDef *value) : value { value } {}

		std::string to_string() const override;
	};

	enum struct Operator {
		lt,
		le,
		eq,
		ge,
		gt,
		plus,
		minus,
		times,
		bitwise_and,
		lshift,
		rshift
	};

	struct BinaryOperation : Rvalue {
		Uptr<Operand> lhs;
		Uptr<Operand> rhs;
		Operator op;

		std::string to_string() const override;
	};

	struct LengthGetter : Rvalue {
		Uptr<Operand> target;
		Opt<Uptr<Operand>> dimension;

		std::string to_string() const override;
	};

	struct FunctionCall : Rvalue {
		Uptr<Operand> callee;
		Vec<Uptr<Operand>> arguments;

		std::string to_string() const override;
	};

	struct NewArray : Rvalue {
		Vec<Uptr<Operand>> dimension_lengths;

		std::string to_string() const override;
	};

	struct NewTuple : Rvalue {
		Uptr<Operand> length;

		std::string to_string() const override;
	};

	// mir::Instruction represents an elementary type-aware option, unlike
	// hir::Instruction which more closely resembles the syntactic construct of
	// an LA instruction
	// interface
	struct Instruction {
		Opt<Place> destination;
		Uptr<Rvalue> rvalue;

		std::string to_string() const;
	};

	struct BasicBlock {
		struct ReturnVoid {};
		struct ReturnVal { Uptr<Operand> return_value; };
		struct Goto { BasicBlock* successor; };
		struct Branch {
			Uptr<Operand> condition;
			BasicBlock *then_block;
			BasicBlock *else_block;
		};
		using Terminator = std::variant<ReturnVoid, ReturnVal, Goto, Branch>;

		// data fields start here
		std::string user_given_label_name; // empty if anonymous
		Vec<Uptr<Instruction>> instructions;
		Terminator terminator;

		BasicBlock(std::string user_given_label_name) :
			user_given_label_name { mv(user_given_label_name) },
			instructions {},
			terminator { ReturnVoid {} }
		{}

		std::string to_string() const;
		std::string get_unambiguous_name() const;
	};

	struct FunctionDef {
		std::string user_given_name; // empty means anonymous
		mir::Type return_type;
		Vec<Uptr<LocalVar>> local_vars;
		Vec<LocalVar *> parameter_vars;
		Vec<Uptr<BasicBlock>> basic_blocks; // the first block is always the entry block

		explicit FunctionDef(std::string user_given_name, mir::Type return_type) :
			user_given_name { mv(user_given_name) }, return_type { return_type }
		{}

		std::string to_string() const;
		std::string get_unambiguous_name() const;
	};

	struct Program {
		Vec<Uptr<FunctionDef>> function_defs;

		std::string to_string() const;
	};
}