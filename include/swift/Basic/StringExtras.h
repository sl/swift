//===--- StringExtras.h - String Utilities ----------------------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// This file provides utilities for working with English words and
// camelCase names.
//
//===----------------------------------------------------------------------===//
#ifndef SWIFT_BASIC_STRINGEXTRAS_HPP
#define SWIFT_BASIC_STRINGEXTRAS_HPP

#include "swift/Basic/LLVM.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Allocator.h"
#include <iterator>
#include <string>

namespace swift {
  /// Describes the kind of preposition a word is.
  enum PrepositionKind {
    PK_None = 0,
    PK_Directional,
    PK_Nondirectional
  };

  /// Determine what kind of preposition the given word is, if any,
  /// ignoring case.
  PrepositionKind getPrepositionKind(StringRef word);

  /// Describes the part of speech of a particular word.
  enum class PartOfSpeech {
    Unknown,
    Preposition,
    Verb,
    Gerund,
  };

  /// Determine the part of speech for the given word.
  PartOfSpeech getPartOfSpeech(StringRef word);

  namespace camel_case {
    class WordIterator;

    /// A bidirectional iterator that walks through the words in a camelCase
    /// string.
    ///
    /// Note that this iterator is not technically conforming bidirectional 
    /// iterator, because it's reference type is not a true reference. But it 
    /// quacks like a duck.
    class WordIterator {
      StringRef String;
      unsigned Position;
      mutable unsigned NextPosition : 31;
      mutable unsigned NextPositionValid : 1;
      mutable unsigned PrevPosition : 31;
      mutable unsigned PrevPositionValid : 1;

      void computeNextPosition() const;
      void computePrevPosition() const;

      /// Proxy used for the arrow operator of the word iterator.
      class ArrowProxy {
        StringRef String;

      public:
        explicit ArrowProxy(StringRef string) : String(string) { }

        const StringRef *operator->() const {
          return &String;
        }
      };

    public:
      typedef StringRef value_type;
      typedef StringRef reference;
      typedef ArrowProxy pointer;
      typedef int difference_type;
      typedef std::bidirectional_iterator_tag iterator_category;

      WordIterator(StringRef string, unsigned position)
        : String(string), Position(position) 
      {
        NextPositionValid = false;
        PrevPositionValid = false;
      }

      StringRef operator*() const {
        if (!NextPositionValid)
          computeNextPosition();

        return String.slice(Position, NextPosition);
      }

      ArrowProxy operator->() const {
        return ArrowProxy(**this);
      }

      WordIterator &operator++() {
        if (!NextPositionValid)
          computeNextPosition();

        // Save the previous position.
        PrevPosition = Position;
        PrevPositionValid = true;

        // Move to the next position.
        Position = NextPosition;
        
        // We don't know what lies ahead.
        NextPositionValid = false;
        return *this;
      }

      WordIterator operator++(int) {
        WordIterator tmp(*this);
        ++(*this);
        return tmp;
      }

      WordIterator &operator--() {
        if (!PrevPositionValid)
          computePrevPosition();

        // Save the next position.
        NextPosition = Position;
        NextPositionValid = true;

        // Move to the previous position.
        Position = PrevPosition;

        // We don't know what lies behind.
        PrevPositionValid = false;

        return *this;
      }

      WordIterator operator--(int) {
        WordIterator tmp(*this);
        --(*this);
        return tmp;
      }

      friend bool operator==(const WordIterator &x, const WordIterator &y) {
        assert(x.String.data() == y.String.data() && 
               x.String.size() == y.String.size() &&
               "comparing word iterators from different strings");
        return x.Position == y.Position;
      }

      friend bool operator!=(const WordIterator &x, const WordIterator &y) {
        return !(x == y);
      }

      /// Retrieve the position of this iterator within the underlying
      /// string.
      unsigned getPosition() const {
        return Position;
      }
    };

    /// Find the first camelCase word in the given string.
    StringRef getFirstWord(StringRef string);

    /// Find the last camelCase word in the given string.
    StringRef getLastWord(StringRef string);

    /// A wrapper that treats a string as a container of camelCase words.
    class Words {
      StringRef String;

    public:
      typedef WordIterator iterator;
      typedef WordIterator const_iterator;
      typedef std::reverse_iterator<WordIterator> reverse_iterator;
      typedef std::reverse_iterator<WordIterator> const_reverse_iterator;
      
      explicit Words(StringRef string) : String(string) { }

      bool empty() const { return String.empty(); }

      iterator begin() const { return WordIterator(String, 0); }
      iterator end() const { return WordIterator(String, String.size()); }

      reverse_iterator rbegin() const { return reverse_iterator(end()); }
      reverse_iterator rend() const { return reverse_iterator(begin()); }
    };

    /// Retrieve the camelCase words in the given string.
    inline Words getWords(StringRef string) { return Words(string); }

    /// Check whether the two words are the same, ignoring the case of the
    /// first letter.
    bool sameWordIgnoreFirstCase(StringRef word1, StringRef word2);

    /// Check whether the first word starts with the second word, ignoring the
    /// case of the first letter.
    bool startsWithIgnoreFirstCase(StringRef word1, StringRef word2);

    /// Lowercase the first word within the given camelCase string.
    ///
    /// \param string The string to lowercase.
    /// \param scratch Scratch buffer used to form the resulting string.
    ///
    /// \returns the string with the first word lowercased. When the
    /// first word is an acronym, the string will be returned
    /// unchanged.
    StringRef toLowercaseWord(StringRef string, SmallVectorImpl<char> &scratch);

    /// Sentence-case the given camelCase string by turning the first
    /// letter into an uppercase letter.
    ///
    /// \param string The string to sentence-case.
    /// \param scratch Scratch buffer used to form the resulting string.
    ///
    /// \returns the string in sentence case.
    StringRef toSentencecase(StringRef string, SmallVectorImpl<char> &scratch);

    /// Drop the class prefix (i..e, a 2-3 character acronym) from the front
    /// of the given string.
    ///
    /// \param string The string whose prefix will be dropped.
    ///
    /// \returns the result of dropping the prefix from \p string, or the
    /// whole string if it has no prefix.
    StringRef dropPrefix(StringRef string);

    /// Append the given string to the given buffer, sentence-casing the string
    /// so that the result reads as separate camelCase words.
    ///
    /// \param buffer The buffer to append to.
    /// \param string The new string to append, which will be sentence-cased.
    ///
    /// \returns the contents of the buffer after appending.
    StringRef appendSentenceCase(SmallVectorImpl<char> &buffer,
                                 StringRef string);

    /// Search the given camelCase string for the first occurrence of
    /// the second string as a complete word.
    ///
    /// \param string The string to search.
    /// \param word The string to search for; must be a single Title word
    /// \returns the index of the start of the match, or String::npos if
    ///   it was not found
    size_t findWord(StringRef string, StringRef word);
  } // end namespace camel_case

/// Describes the role that a particular name has within a
/// signature, which can affect how we omit needless words.
enum class NameRole {
  /// The base name of a function or method.
  BaseName,

  /// The first parameter of a function or method.
  FirstParameter,

  // Subsequent parameters in a function or method.
  SubsequentParameter,

  // The name of a property.
  Property,

  // A partial name; used internally.
  Partial,
};

/// Describes the name of a type as is used for omitting needless
/// words.
struct OmissionTypeName {
  /// The name of the type.
  StringRef Name;

  /// For a collection type, the name of the element type.
  StringRef CollectionElement;

  /// Construct a type name.
  OmissionTypeName(StringRef name = StringRef(),
                   StringRef collectionElement = StringRef())
    : Name(name), CollectionElement(collectionElement) { }

  /// Construct a type name.
  OmissionTypeName(const char * name,
                   StringRef collectionElement = StringRef())
    : Name(name), CollectionElement(collectionElement) { }

  /// Determine whether the type name is empty.
  bool empty() const { return Name.empty(); }

  friend bool operator==(const OmissionTypeName &lhs,
                         const OmissionTypeName &rhs) {
    return lhs.Name == rhs.Name &&
      (lhs.CollectionElement.empty() ||
       rhs.CollectionElement.empty() ||
       lhs.CollectionElement == rhs.CollectionElement);
  }

  friend bool operator!=(const OmissionTypeName &lhs,
                         const OmissionTypeName &rhs) {
    return !(lhs == rhs);
  }
};

/// Match the given type name at the beginning of the given name,
/// returning the remainder of the name.
///
/// For example, matching "stringByAppendingString" to the type "NSString"
/// would produce "ByAppendingString".
StringRef matchLeadingTypeName(StringRef name, OmissionTypeName typeName);

/// Scratch space used for returning a set of StringRefs.
class StringScratchSpace {
  llvm::BumpPtrAllocator Allocator;

public:
  StringRef copyString(StringRef string);
};

/// Attempt to omit needless words from the given name based on the
/// name of the type associated with the name.
///
/// \param name The name from which we will attempt to remove needless
/// words.
///
/// \param typeName The name of the type of the entity being described.
///
/// \param role The role of the name, e.g., a parameter name, property
/// name, base name of a function, etc.
///
/// \param scratch Scratch space that will be used for modifications beyond
/// just chopping names.
///
/// \returns the updated name.
StringRef omitNeedlessWords(StringRef name, OmissionTypeName typeName,
                            NameRole role, StringScratchSpace &scratch);

/// Omit needless words for a function, method, or initializer.
///
/// \param baseName The base name of the function. This value may be
/// changed if any words are removed.
///
/// \param argNames The names of the arguments to the function. The
/// values in this array may be changed if any words are removed.
///
/// \param resultType The name of the result type of the function.
///
/// \param contextType The name of the type of the enclosing context,
/// e.g., the class name.
///
/// \param paramTypes The names of the parameter types for the
/// function.
///
/// \param returnsSelf Whether the result of the function is 'Self'
/// (in Swift) or 'instancetype' (in Objective-C).
///
/// \param scratch Scratch space that will be used for modifications beyond
/// just chopping names.
///
/// \returns true if any words were omitted, false otherwise.
bool omitNeedlessWords(StringRef &baseName,
                       MutableArrayRef<StringRef> argNames,
                       OmissionTypeName resultType,
                       OmissionTypeName contextType,
                       ArrayRef<OmissionTypeName> paramTypes,
                       bool returnsSelf,
                       StringScratchSpace &scratch);
}

#endif // LLVM_SWIFT_BASIC_STRINGEXTRAS_HPP
