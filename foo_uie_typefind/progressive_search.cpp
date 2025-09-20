#include "pch.h"

#include "progressive_search.h"

namespace {

auto split_into_words(std::wstring_view text)
{
    return text | ranges::views::split_when([](auto&& character) { return std::iswspace(character); })
        | ranges::views::filter([](auto&& word) { return !ranges::empty(word); })
        | ranges::views::transform([](auto&& word) {
              const auto size = ranges::distance(word);
              return size > 0 ? std::wstring_view(&*ranges::begin(word), size) : std::wstring_view{};
          });
}

bool starts_with(std::wstring_view full_string, std::wstring_view partial_string, bool ignore_symbols)
{
    const auto match_index = FindNLSStringEx(LOCALE_NAME_USER_DEFAULT,
        FIND_STARTSWITH | LINGUISTIC_IGNOREDIACRITIC | NORM_IGNORECASE | NORM_IGNOREWIDTH | NORM_LINGUISTIC_CASING
            | (ignore_symbols ? NORM_IGNORESYMBOLS : 0),
        full_string.data(), gsl::narrow<int>(full_string.size()), partial_string.data(),
        gsl::narrow<int>(partial_string.size()), nullptr, nullptr, nullptr, 0);

    return match_index >= 0;
}

} // namespace

void typefind_panel::ProgressiveSearch::run()
{
    bool clear_selection = m_mode == SearchMode::mode_query;

    if (m_mode == SearchMode::mode_query && !m_string.empty()) {
        const auto filter_api = search_filter_manager_v2::get();
        const auto string_utf8 = mmh::to_utf8(m_string);
        search_filter_v2::ptr filter;

        try {
            filter = filter_api->create_ex(string_utf8.c_str(), new service_impl_t<completion_notify_dummy>(),
                search_filter_manager_v2::KFlagSuppressNotify);
            clear_selection = false;
        } catch (const pfc::exception&) {
        }

        if (filter.is_valid())
            filter->test_multi(m_entries, m_filter.get_ptr());
    }

    if (clear_selection || m_string.empty()) {
        m_playlist_api->playlist_clear_selection(m_active);
        return;
    }

    std::optional<size_t> candidate_focus;

    const auto terms = m_mode == SearchMode::mode_match_words_beginning_formatted_title
        ? split_into_words(m_string) | ranges::to<std::vector<std::wstring_view>>()
        : std::vector<std::wstring_view>{};

    for (const auto index : std::ranges::views::iota(size_t{}, m_entries.get_count())) {
        if (!m_filter[index])
            continue;

        switch (m_mode) {
        case SearchMode::mode_query:
            candidate_focus = index;
            break;
        case SearchMode::mode_match_beginning_formatted_title:
            if (starts_with(m_formatted[index], m_string, m_ignore_symbols)) {
                if (!candidate_focus)
                    candidate_focus = index;
            } else {
                m_filter[index] = false;
            }
            break;
        case SearchMode::mode_match_words_beginning_formatted_title: {
            const auto& target_string = m_formatted[index];
            auto target_words = split_into_words(target_string);

            const auto all_terms_match = ranges::all_of(terms, [this, &target_words](auto&& term) {
                return ranges::any_of(target_words,
                    [this, term](auto&& target_word) { return starts_with(target_word, term, m_ignore_symbols); });
            });

            if (all_terms_match && !terms.empty()) {
                if (!candidate_focus)
                    candidate_focus = index;
            } else {
                m_filter[index] = false;
            }
            break;
        }
        }
    }

    const auto existing_focus = fbh::as_optional(m_playlist_api->playlist_get_focus_item(m_active));

    if (candidate_focus && !(existing_focus && *existing_focus < m_filter.get_size() && m_filter[*existing_focus])) {
        m_playlist_api->playlist_set_selection(m_active, bit_array_true(), bit_array_one(*candidate_focus));
        m_playlist_api->playlist_set_focus_item(m_active, *candidate_focus);
    } else if (candidate_focus && existing_focus) {
        m_playlist_api->playlist_set_selection(m_active, bit_array_true(), bit_array_one(*existing_focus));
        m_playlist_api->playlist_ensure_visible(m_active, *existing_focus);
    }
}
