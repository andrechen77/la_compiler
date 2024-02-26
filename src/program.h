#pragma once

#include "std_alias.h"
#include <variant>
#include <string>
#include <string_view>
#include <iostream>

namespace La::program {
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

	template<typename Item> class Scope;
	struct Nameable;

	// interface
	struct Expr {
		int line_number;

		virtual void bind_to_scope(Scope<Nameable> &agg_scope) = 0;
		virtual std::string to_string() const = 0;
	};

	// instantiations must implement the virtual methods
	template<typename Item>
	class ItemRef : public Expr {
		std::string free_name; // the original name
		Item *referent_nullable;

		public:

		ItemRef(std::string free_name) :
			free_name { mv(free_name) },
			referent_nullable { nullptr }
		{}

		void bind_to_scope(Scope<Nameable> &agg_scope) override;
		void bind(Item *referent) {
			this->referent_nullable = referent;
		}
		Opt<Item *> get_referent() const {
			if (this->referent_nullable) {
				return this->referent_nullable;
			} else {
				return {};
			}
		}
		const std::string &get_ref_name() const {
			if (this->referent_nullable) {
				return this->referent_nullable->get_name();
			} else {
				return this->free_name;
			}
		}
		std::string to_string() const override;
	};

	struct NumberLiteral : Expr {
		int64_t value;

		NumberLiteral(int64_t value) : value { value } {}

		void bind_to_scope(Scope<Nameable> &agg_scope) override;
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
	Operator str_to_op(std::string_view str);
	std::string to_string(Operator op);

	struct BinaryOperation : Expr {
		Uptr<Expr> lhs;
		Uptr<Expr> rhs;
		Operator op;

		void bind_to_scope(Scope<Nameable> &agg_scope) override;
		std::string to_string() const override;
	};

	struct IndexingExpr : Expr {
		Uptr<Expr> target;
		Vec<Uptr<Expr>> indices;

		void bind_to_scope(Scope<Nameable> &agg_scope) override;
		std::string to_string() const override;
	};

	struct LengthGetter : Expr {
		Uptr<Expr> target;
		Opt<Uptr<Expr>> dimension;

		void bind_to_scope(Scope<Nameable> &agg_scope) override;
		std::string to_string() const override;
	};

	struct FunctionCall : Expr {
		Uptr<Expr> callee;
		Vec<Uptr<Expr>> arguments;

		void bind_to_scope(Scope<Nameable> &agg_scope) override;
		std::string to_string() const override;
	};

	struct NewArray : Expr {
		Vec<Uptr<Expr>> dimension_lengths;

		void bind_to_scope(Scope<Nameable> &agg_scope) override;
		std::string to_string() const override;
	};

	struct NewTuple : Expr {
		Uptr<Expr> length;

		void bind_to_scope(Scope<Nameable> &agg_scope) override;
		std::string to_string() const override;
	};

	// interface
	struct Instruction {
		virtual void bind_to_scope(Scope<Nameable> &scope) = 0;
		virtual std::string to_string() const = 0;
	};

	struct InstructionDeclaration : Instruction {
		Type type;
		std::string variable_name;

		void bind_to_scope(Scope<Nameable> &scope) override;
		std::string to_string() const override;
	};

	struct InstructionAssignment : Instruction {
		// the destination is optional only for the pure call instruction
		Opt<Uptr<IndexingExpr>> maybe_dest;
		Uptr<Expr> source;

		void bind_to_scope(Scope<Nameable> &scope) override;
		std::string to_string() const override;
	};

	struct InstructionLabel : Instruction {
		std::string label_name;

		void bind_to_scope(Scope<Nameable> &scope) override;
		std::string to_string() const override;
	};

	struct InstructionReturn : Instruction {
		Opt<Uptr<Expr>> return_value;

		void bind_to_scope(Scope<Nameable> &scope) override;
		std::string to_string() const override;
	};

	struct InstructionBranchUnconditional : Instruction {
		std::string label_name; // TODO consider making it an ItemRef

		void bind_to_scope(Scope<Nameable> &scope) override;
		std::string to_string() const override;
	};

	struct InstructionBranchConditional : Instruction {
		Uptr<Expr> condition;
		std::string then_label_name; // TODO consider making it an ItemRef
		std::string else_label_name; // TODO consider making it an ItemRef

		void bind_to_scope(Scope<Nameable> &scope) override;
		std::string to_string() const override;
	};

	// A Scope represents a namespace of Items that the ItemRefs care about.
	// A Scope does not own any of the Items it maps to.
	// `(name, item)` pairs in this->dict represent Items defined in this scope
	// under `name`.
	// `(name, ItemRef *)` in free_referrers represents that that ItemRef has
	// refers to `name`, but that it is a free name (unbound to anything in this
	// scope)
	template<typename Item>
	class Scope {
		// If a Scope has a parent, then it cannot have any
		// free_refs; they must have been transferred to the parent.
		Opt<Scope *> parent;
		Map<std::string, Item *> dict;
		Map<std::string, Vec<ItemRef<Item> *>> free_refs;

		public:

		Scope() : parent {}, dict {}, free_refs {} {}

		Vec<Item *> get_all_items() const {
			Vec<Item *> result;
			if (this->parent) {
				result = mv(static_cast<const Scope *>(*this->parent)->get_all_items());
			}
			for (const auto &[name, item] : this->dict) {
				result.push_back(item);
			}
			return result;
		}

		/* std::vector<Item> get_all_items() {
			std::vector<Item> result;
			if (this->parent) {
				result = std::move((*this->parent)->get_all_items());
			}
			for (auto &[name, item] : this->dict) {
				result.push_back(item);
			}
			return result;
		} */

		// returns whether the ref was immediately bound or was left as free
		bool add_ref(ItemRef<Item> &item_ref) {
			std::string_view ref_name = item_ref.get_ref_name();

			Opt<Item *> maybe_item = this->get_item_maybe(ref_name);
			if (maybe_item) {
				// bind the ref to the item
				item_ref.bind(*maybe_item);
				return true;
			} else {
				// there is no definition of this name in the current scope
				this->push_free_ref(item_ref);
				return false;
			}
		}

		// Adds the specified item to this scope under the specified name,
		// resolving all free refs who were depending on that name. Dies if
		// there already exists an item under that name.
		void resolve_item(std::string name, Item *item) {
			auto existing_item_it = this->dict.find(name);
			if (existing_item_it != this->dict.end()) {
				std::cerr << "name conflict: " << name << std::endl;
				exit(-1);
			}

			const auto [item_it, _] = this->dict.insert(std::make_pair(name, item));
			auto free_refs_vec_it = this->free_refs.find(name);
			if (free_refs_vec_it != this->free_refs.end()) {
				for (ItemRef<Item> *item_ref_ptr : free_refs_vec_it->second) {
					item_ref_ptr->bind(item_it->second);
				}
				this->free_refs.erase(free_refs_vec_it);
			}
		}

		// In addition to using free names like normal, clients may also use
		// this method to define an Item at the same time that it is used.
		// (kinda like python variable declaration).
		// The below conditional inclusion trick doesn't work because
		// gcc-toolset-11 doesn't seem to respect SFINAE, so just allow all
		// instantiation sto use it and hope for the best.
		// template<typename T = std::enable_if_t<DefineOnUse>>
		/* Item get_item_or_create(const std::string_view &name) {
			std::optional<Item *> maybe_item_ptr = get_item_maybe(name);
			if (maybe_item_ptr) {
				return *maybe_item_ptr;
			} else {
				const auto [item_it, _] = this->dict.insert(std::make_pair(
					std::string(name),
					Item(name)
				));
				return &item_it->second;
			}
		} */

		std::optional<Item *> get_item_maybe(std::string_view name) {
			auto item_it = this->dict.find(name);
			if (item_it != this->dict.end()) {
				return std::make_optional<Item *>(item_it->second);
			} else {
				if (this->parent) {
					return (*this->parent)->get_item_maybe(name);
				} else {
					return {};
				}
			}
		}

		/* void remove_item(Item *item) {
			for (auto it = this->dict.begin(); it != this->dict.end(); ++it) {
				if (&it->second == item) {
					dict.erase(it);
					break;
				}
			}
		} */

		// Sets the given Scope as the parent of this Scope, transferring all
		// current and future free names to the parent. If this scope already
		// has a parent, dies.
		void set_parent(Scope &parent) {
			if (this->parent) {
				std::cerr << "this scope already has a parent oops\n";
				exit(1);
			}

			this->parent = std::make_optional<Scope *>(&parent);

			for (auto &[name, our_free_refs_vec] : this->free_refs) {
				for (ItemRef<Item> *our_free_ref : our_free_refs_vec) {
					// TODO optimization here is possible; instead of using the
					// public API of the parent we can just query the dictionary
					// directly
					(*this->parent)->add_ref(*our_free_ref);
				}
			}
			this->free_refs.clear();
		}

		// returns whether free refs exist in this scope for the given name
		Vec<ItemRef<Item> *> get_free_refs() const {
			std::vector<ItemRef<Item> *> result;
			for (auto &[name, free_refs_vec] : this->free_refs) {
				result.insert(result.end(), free_refs_vec.begin(), free_refs_vec.end());
			}
			return result;
		}

		// returns the free names exist in this scope
		Vec<std::string> get_free_names() const {
			Vec<std::string> result;
			for (auto &[name, free_refs_vec] : this->free_refs) {
				result.push_back(name);
			}
			return result;
		}

		// // binds all free names to the given item
		/* void fake_bind_frees(Item *item_ptr) {
			for (auto &[name, free_refs_vec] : this->free_refs) {
				for (ItemRef *item_ref_ptr : free_refs_vec) {
					// TODO we should be allowed to print this
					// std::cerr << "fake-bound free name: " << item_ref_ptr->get_ref_name() << "\n";
					item_ref_ptr->bind(item_ptr);
				}
			}
			this->free_refs.clear();
		} */

		private:

		// Given an item_ref, exposes it as a ref with a free name. This may
		// be caught by the parent Scope and resolved, or the parent might
		// also expose it as a free ref recursively.
		void push_free_ref(ItemRef<Item> &item_ref) {
			std::string_view ref_name = item_ref.get_ref_name();
			if (this->parent) {
				(*this->parent)->add_ref(item_ref);
			} else {
				this->free_refs[std::string(ref_name)].push_back(&item_ref);
			}
		}
	};

	// something that can be referred to by a simple name
	struct Nameable {
		virtual const std::string &get_name() const = 0;
	};

	struct Variable : Nameable {
		std::string name;
		Type type;

		const std::string &get_name() const override { return this->name; }
	};

	struct LaFunction : Nameable {
		std::string name;
		Type return_type;
		Vec<Uptr<Instruction>> instructions;
		Vec<Uptr<Variable>> vars;
		Vec<Variable *> parameter_vars;
		Scope<Nameable> scope;

		explicit LaFunction(std::string name, Type return_type);

		const std::string &get_name() const override { return this->name; }
		std::string to_string() const;
	};

	struct ExternalFunction : Nameable {
		std::string name;
		int num_parameters;

		const std::string &get_name() const override { return this->name; }
	};

	struct Program {
		Vec<Uptr<LaFunction>> la_functions;
		Vec<Uptr<ExternalFunction>> external_functions;
		Scope<Nameable> scope;

		std::string to_string() const;
		void add_la_function(Uptr<LaFunction> la_function);
	};
}
