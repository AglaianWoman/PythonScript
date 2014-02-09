#ifndef REPLACER_20140209_H
#define REPLACER_20140209_H
#include "ReplaceEntry.h"
#include "Match.h"
#include "UTF8Iterator.h"
#include "ANSIIterator.h"
#include "ConstString.h"
#include "UtfConversion.h"

namespace NppPythonScript 
{
    typedef ReplaceEntry* (*matchConverter)(const char *, Match *, void *state);
    
    using UtfConversion::toStringType;



    typedef enum _python_re_flags {
        python_re_flag_normal = 0,
		python_re_flag_ignorecase = 2,
        python_re_flag_locale = 4,
        python_re_flag_multiline = 8,
        python_re_flag_dotall = 16,
        // Internal flags
        python_re_flag_literal = 0x80000000
	} python_re_flags;


    
template <class CharTraitsT>
class BoostRegexGroupDetail : public GroupDetail
{
public:
    BoostRegexGroupDetail(const boost::sub_match<typename CharTraitsT::text_iterator_type>& subMatch)
		: m_subMatch(subMatch)
	{}
	int start() const { return  m_subMatch.first.pos(); }
	int end() const { return m_subMatch.second.pos(); }

private:
    boost::sub_match<typename CharTraitsT::text_iterator_type> m_subMatch;
};

template<class CharTraitsT>
class BoostRegexMatch : public Match
{
public:
    BoostRegexMatch(const char *text, boost::match_results<typename CharTraitsT::text_iterator_type>* match)
		: m_text(text),
          m_match(match)
	{}

    BoostRegexMatch(const char *text)
		: m_text(text),
          m_match(NULL)
	{}

    virtual ~BoostRegexMatch();
    BoostRegexMatch& operator= (BoostRegexMatch& rhs) {
        m_text = rhs.m_text;
        m_match = rhs.m_match;

        /* We explicitely don't copy the list, as the allocatedGroupDetails will simply be destructed when this object gets destroyed.
         * In theory, this would be bad, as we would delete the allocated GroupDetail objects when this object is deleted, 
         * even though the various groups may still be in use.  
         * In practice however, these GroupDetails don't actually live as long as this object, as we've created a ReplaceEntry 
         * by the time this object gets destroyed, and have no need for the allocated GroupDetails any more.
         */ 
	}
    
	void setMatchResults(boost::match_results<typename CharTraitsT::text_iterator_type>* match) { m_match = match; }

	virtual int groupCount() { return m_match->size(); }

    virtual GroupDetail* group(int groupNo);
    virtual GroupDetail* groupName(const char *groupName);

    virtual std::string getTextForGroup(GroupDetail* group);

    virtual void expand(const char* format, char **result, int *resultLength);

private: 
    const char *m_text;
    boost::match_results<typename CharTraitsT::text_iterator_type>* m_match;
    std::list<BoostRegexGroupDetail<CharTraitsT>* > m_allocatedGroupDetails;

    static void deleteEntry(BoostRegexGroupDetail<CharTraitsT>*);
};



template <class CharTraitsT>
void BoostRegexMatch<CharTraitsT>::deleteEntry(BoostRegexGroupDetail<CharTraitsT>* entry)
{
    delete entry;
}

template <class CharTraitsT>
BoostRegexMatch<CharTraitsT>::~BoostRegexMatch() 
{
    for_each(m_allocatedGroupDetails.begin(), m_allocatedGroupDetails.end(), deleteEntry);
}

template <class CharTraitsT>
GroupDetail* BoostRegexMatch<CharTraitsT>::group(int groupNo) 
{
    BoostRegexGroupDetail<CharTraitsT>* groupDetail = new BoostRegexGroupDetail<CharTraitsT>((*m_match)[groupNo]);
    m_allocatedGroupDetails.push_back(groupDetail);
    return groupDetail;
}

template <class CharTraitsT>
GroupDetail* BoostRegexMatch<CharTraitsT>::groupName(const char *groupName) 
{
    CharTraitsT::string_type groupNameU32 = toStringType<CharTraitsT::string_type>(ConstString<char>(groupName));
    BoostRegexGroupDetail<CharTraitsT>* groupDetail =  new BoostRegexGroupDetail<CharTraitsT>((*m_match)[groupNameU32.c_str()]);
    m_allocatedGroupDetails.push_back(groupDetail);
    return groupDetail;
}

template <class CharTraitsT>
void BoostRegexMatch<CharTraitsT>::expand(const char *format, char **result, int *resultLength)
{
    CharTraitsT::string_type resultString = m_match->format(format);

    std::string charResult(CharTraitsT::toCharString(resultString));

    *resultLength = charResult.size();
    *result = new char[(*resultLength) + 1];
    memcpy(*result, charResult.c_str(), *resultLength);
    (*result)[*resultLength] = '\0';
}

template <class CharTraitsT>
typename std::string BoostRegexMatch<CharTraitsT>::getTextForGroup(GroupDetail* groupDetail)
{
    return std::string(m_text + groupDetail->start(), m_text + groupDetail->end());
}

    class Utf8CharTraits {
	public:
        typedef std::basic_string<U32> string_type;
        typedef boost::basic_regex<U32, u32_regex_traits> regex_type;
        typedef boost::regex_iterator<UTF8Iterator, U32, u32_regex_traits> regex_iterator_type;
        typedef UTF8Iterator text_iterator_type;

		static std::basic_string<char> toCharString(const string_type& source) { 
            // TODO:  There's probably more copying, allocing and deleting going on here than there actually needs to be
            // We just want a u32string to utf8 char*
		    return std::basic_string<char>(UtfConversion::toUtf8(ConstString<U32>(source)));
		}
	};

    class AnsiCharTraits {
	public:
        typedef std::string string_type;
        typedef boost::regex regex_type;
        typedef boost::regex_iterator<ANSIIterator, char> regex_iterator_type;
        typedef ANSIIterator text_iterator_type;

        static std::basic_string<char> toCharString(const string_type& source) { 
		    return source;
		}
	};

    template <class CharTraitsT>
	class Replacer {

	public:
		Replacer()
			: m_flags(python_re_flag_normal)
		{ }

        explicit Replacer(python_re_flags flags) 
			: m_flags(flags) 
		{}

        bool startReplace(const char *text, const int textLength, const char *search, matchConverter converter, void *converterState, std::list<ReplaceEntry*>& replacements);
        bool startReplace(const char *text, const int textLength, const char *search, const char *replace, std::list<ReplaceEntry*>& replacements);

	private:
        static ReplaceEntry* matchToReplaceEntry(const char *text, Match *match, void *state);

        boost::regex_constants::syntax_option_type getSyntaxFlags() 
		{ return (m_flags & python_re_flag_literal) 
			        ? boost::regex_constants::literal
					: boost::regex_constants::normal;
		}

        python_re_flags m_flags;
        const char *m_replaceFormat;
	};

    
template<class CharTraitsT>
ReplaceEntry* NppPythonScript::Replacer<CharTraitsT>::matchToReplaceEntry(const char * /* text */, Match *match, void *state)
{
    // TODO: state is replacer instance, and contains the replacement string
	// need to add format call in here, 
    Replacer *replacer = reinterpret_cast<Replacer*>(state);
    char *replacement;
    int replacementLength;
	match->expand(replacer->m_replaceFormat, &replacement, &replacementLength);

    GroupDetail *fullMatch = match->group(0);
    ReplaceEntry* replaceEntry = new ReplaceEntry(fullMatch->start(), fullMatch->end(), replacement, replacementLength);
    delete [] replacement;
    return replaceEntry;
}

template<class CharTraitsT>
bool NppPythonScript::Replacer<CharTraitsT>::startReplace(const char *text, const int textLength, const char *search,
    const char *replace, 
    std::list<ReplaceEntry*>& replacements)
{
    m_replaceFormat = replace;
    return startReplace(text, textLength, search, matchToReplaceEntry, this, replacements);
}

template<class CharTraitsT>
bool NppPythonScript::Replacer<CharTraitsT>::startReplace(const char *text, const int textLength, const char *search, 
	matchConverter converter,
    void *converterState,
	std::list<ReplaceEntry*> &replacements) {

    boost::regex_constants::syntax_option_type syntax_flags = getSyntaxFlags();

    CharTraitsT::regex_type r = CharTraitsT::regex_type(toStringType<CharTraitsT::string_type>(ConstString<char>(search)), syntax_flags);

    CharTraitsT::text_iterator_type start(text, 0, textLength);
    CharTraitsT::text_iterator_type end(text, textLength, textLength);
    CharTraitsT::regex_iterator_type iteratorEnd;
    BoostRegexMatch<CharTraitsT> match(text);
    for(CharTraitsT::regex_iterator_type it(start, end, r); it != iteratorEnd; ++it) {
        boost::match_results<CharTraitsT::text_iterator_type> boost_match_results(*it);

        match.setMatchResults(&boost_match_results); 
        ReplaceEntry* entry = converter(text, &match, converterState);
        replacements.push_back(entry);
	}

    return false;
}

}

#endif // REPLACER_20140209_H   