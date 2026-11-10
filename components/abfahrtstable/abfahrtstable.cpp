#include "abfahrtstable.h"
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace esphome::abfahrtstable {

using tessera::Font;
namespace ui = tessera::ui;

// One departure, as the helper sensor hands it over: which line, where it goes, when it leaves (seconds since 1970), and
// how many minutes late it runs. The helper has already filtered, merged and sorted several line+destination pairs into
// these parallel lists, so the board can be heterogeneous (every row its own line and destination).
struct Departure {
  std::string line, to;
  uint32_t at = 0;
  int delay = 0;
};

// The tile. A single cell: the next departure big, with its line and destination above it and the two after it below.
// Two columns or more: a row per departure, as many as fit, each with its own line badge and destination (the board
// mixes lines). The helper sends new times when Home Assistant has them; the countdown runs on the screen's own clock in
// whole minutes (never seconds), and only a text that changes is set again.
class BoardTile : public tessera::Tile {
 public:
  explicit BoardTile(const tessera::Plugin *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    width_ = c.width;
    height_ = c.height;
    late_ = c.options["late"] | true;
    header_ = c.options["header"] | true;
    max_rows_ = c.options["max_rows"] | 0;
    // How the departure time reads: minutes only (default, "now" under a minute), clock only (24h), or both
    // ("5 min · 14:20"). A single choice, so it can never land on the meaningless "neither" an independent pair could.
    const std::string tf = c.options["time_format"] | "minutes";
    show_clock_ = (tf == "clock" || tf == "both");
    show_minutes_ = !show_clock_ || tf == "both";   // minutes unless clock-only; keeps the time text never empty
    departures_.reserve(MAX_DEPARTURES);   // grow once, here, not on a data update or a tick
    next_.reserve(MAX_DEPARTURES);
    list_ = c.columns >= 2;
    head_ = std::max(ui::line_height(Font::TITLE), ui::line_height(Font::BODY)) + ui::px(4);
    gap_ = ui::px(ui::large() ? 8 : 5);
    top_ = 0;
    if (list_) {
      const int row = head_ + gap_;
      // A title line (the sensor's name) above the rows when it is wanted and there is room for it and two rows under
      // it. It only tells the board apart from another; every row still carries its own line and destination.
      const int title_h = ui::line_height(Font::BODY);
      if (header_ && height_ >= title_h + gap_ + 2 * row) {
        title_ = ui::label(c.parent, Font::BODY, theme::MUTED);
        lv_label_set_long_mode(title_, LV_LABEL_LONG_DOT);
        lv_obj_set_size(title_, width_, title_h);
        lv_obj_set_pos(title_, 0, 0);
        top_ = title_h + gap_;
      }
      const int fit = (height_ - top_ + gap_) / row;
      // The helper merges at most MAX_DEPARTURES into its lists, so more rows than that would always stand empty: cap
      // the row objects there (fewer LVGL objects, less layout memory) even when a huge tile could fit more.
      const int want = max_rows_ > 0 ? std::min(max_rows_, static_cast<int>(MAX_DEPARTURES)) : static_cast<int>(MAX_DEPARTURES);
      rows_.resize(std::max(1, std::min(want, fit)));
      for (size_t i = 0; i < rows_.size(); ++i) make_row(rows_[i], c.parent, top_ + static_cast<int>(i) * row);
      note_ = ui::label(c.parent, Font::BODY, theme::MUTED);
      // One line high: a label of LVGL's own height wraps what does not fit onto a second line under the card.
      lv_obj_set_size(note_, width_, ui::line_height(Font::BODY));
      lv_obj_set_pos(note_, 0, height_ - ui::line_height(Font::BODY));
    } else {
      rows_.resize(1);
      make_row(rows_[0], c.parent, 0);
      big_ = ui::label(c.parent, Font::VALUE, theme::INK);
      lv_obj_set_width(big_, width_);
      later_ = ui::label(c.parent, Font::BODY, theme::MUTED);
      lv_obj_set_size(later_, width_, ui::line_height(Font::BODY));   // one line, cut with dots (as note_)
      lv_obj_set_pos(later_, 0, height_ - ui::line_height(Font::BODY));
    }
  }

  void on_state(JsonObjectConst data) override {
    departures_.clear();
    // The helper's parallel lists arrive under the entity's attributes (docs/FIRMWARE_API.md, "What on_state gets").
    JsonArrayConst ats = data["attributes"]["ats"].as<JsonArrayConst>();
    JsonArrayConst lines = data["attributes"]["lines"].as<JsonArrayConst>();
    JsonArrayConst tos = data["attributes"]["tos"].as<JsonArrayConst>();
    JsonArrayConst delays = data["attributes"]["delays"].as<JsonArrayConst>();
    const size_t n = std::min<size_t>(ats.size(), MAX_DEPARTURES);
    if (departures_.capacity() < n) departures_.reserve(MAX_DEPARTURES);
    for (size_t i = 0; i < n; ++i) {
      Departure d;
      d.at = ats[i] | 0u;
      d.line = i < lines.size() ? (lines[i] | "") : "";
      d.to = i < tos.size() ? (tos[i] | "") : "";
      d.delay = i < delays.size() ? (delays[i] | 0) : 0;
      if (d.at) departures_.push_back(std::move(d));
    }
    std::sort(departures_.begin(), departures_.end(), [](const Departure &a, const Departure &b) { return a.at < b.at; });
    // The entity is there once Home Assistant has sent its state; before a sensor is chosen the tile gets {} and no state.
    has_entity_ = !data["state"].isNull();
    wrong_ = std::string(data["wait"] | "") == "wrong_entity";
    stale_ = data["attributes"]["stale"] | false;
    name_ = data["name"] | "";
    if (title_) ui::set_text(title_, name_.empty() ? plugin_->text("board") : name_);
  }

  void on_tick(uint32_t now) override {
    // The departures still to come: the second has not passed yet (a small grace so "now" lingers a moment). next_ is a
    // member and only cleared here, so the per-second tick allocates nothing once it has grown to its handful of slots.
    next_.clear();
    for (const auto &d : departures_)
      if (now && d.at + 30 >= now) next_.push_back(&d);
    const char *why = (!has_entity_ || wrong_) ? "choose_sensor" : !now ? "waiting" : next_.empty() ? "none" : nullptr;
    if (list_) paint_list(next_, why, now);
    else paint_single(next_, why, now);
  }

  void on_theme() override {
    for (auto &r : rows_) {
      lv_obj_set_style_bg_color(r.badge, ui::color(theme::ACCENT), 0);
      ui::set_color(r.number, theme::ON_ACCENT);
      ui::set_color(r.to, list_ ? theme::INK : theme::MUTED);
      if (r.when) ui::set_color(r.when, theme::INK);
    }
    if (title_) ui::set_color(title_, theme::MUTED);
    if (big_) ui::set_color(big_, theme::INK);
    if (later_) ui::set_color(later_, theme::MUTED);
    if (note_) ui::set_color(note_, theme::MUTED);
  }

 private:
  struct Row {
    lv_obj_t *badge{}, *number{}, *to{}, *when{};
    int y = 0;
  };

  void make_row(Row &r, lv_obj_t *parent, int y) {
    r.y = y;
    r.badge = ui::block(parent, theme::ACCENT);
    r.number = ui::label(r.badge, Font::TITLE, theme::ON_ACCENT);
    lv_label_set_long_mode(r.number, LV_LABEL_LONG_CLIP);
    lv_obj_center(r.number);
    r.to = ui::label(parent, Font::BODY, list_ ? theme::INK : theme::MUTED);
    if (list_) {
      r.when = ui::label(parent, Font::TITLE, theme::INK);
      lv_obj_set_style_text_align(r.when, LV_TEXT_ALIGN_RIGHT, 0);
    }
  }

  // The line's badge as wide as its number, and the rest of the row beside it.
  void place_row(Row &r, const std::string &line, const std::string &to, const std::string &when) {
    const bool shown = !line.empty();
    lv_obj_set_style_opa(r.badge, shown ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    ui::set_text(r.number, line);
    const int pad = ui::px(ui::large() ? 8 : 5);
    const int badge_w = shown ? std::min(width_ / 2, ui::text_width(line, Font::TITLE) + 2 * pad) : 0;
    lv_obj_set_size(r.badge, std::max(1, badge_w), head_);
    lv_obj_set_pos(r.badge, 0, r.y);
    lv_obj_center(r.number);
    const int x = shown ? badge_w + ui::px(6) : 0;
    int right = width_;
    if (r.when) {
      const int when_w = when.empty() ? 0 : ui::text_width(when, Font::TITLE) + ui::px(2);
      ui::set_text(r.when, when);
      lv_obj_set_size(r.when, std::max(1, when_w), ui::line_height(Font::TITLE));
      lv_obj_set_pos(r.when, width_ - when_w, r.y + (head_ - ui::line_height(Font::TITLE)) / 2);
      right = width_ - when_w - (when_w ? ui::px(6) : 0);
    }
    ui::set_text(r.to, to);
    lv_obj_set_size(r.to, std::max(1, right - x), ui::line_height(Font::BODY));
    lv_obj_set_pos(r.to, x, r.y + (head_ - ui::line_height(Font::BODY)) / 2);
  }

  // Where it goes, and how late it runs when the helper says a minute or more.
  std::string with_delay(const Departure &d) const {
    if (!late_ || d.delay <= 0) return d.to;
    return d.to + "  " + tessera::format(plugin_->text("late"), static_cast<long>(d.delay));
  }

  // How a departure's time reads, every row the same way, in whole minutes — never seconds. Minutes: "5 min" until it
  // leaves, "now" under a minute, and still minutes (never a time of day) however far off it is. Clock: the time of day
  // (24h). Both: "5 min · 14:20". The screen's own clock drives it, but only the minute shows, so the text changes once
  // a minute, not once a second.
  std::string when_text(uint32_t at, uint32_t now) const {
    std::string mins, clock;
    if (show_minutes_)
      mins = at <= now + 59 ? plugin_->text("now")
                            : tessera::format(plugin_->text("minutes"), static_cast<long>((at - now) / 60));
    if (show_clock_) clock = tessera::clock_text(at);
    if (show_minutes_ && show_clock_) return mins + " · " + clock;
    return show_clock_ ? clock : mins;
  }

  void paint_single(const std::vector<const Departure *> &next, const char *why, uint32_t now) {
    Row &r = rows_[0];
    if (why) {
      place_row(r, "", "", "");
      big_text(plugin_->text(why), Font::BODY_LARGE, theme::MUTED);
      ui::set_text(later_, "");
      return;
    }
    const Departure &d = *next[0];
    place_row(r, d.line, with_delay(d), "");
    const std::string when = when_text(d.at, now);
    // The face is chosen for the shape of the time, not its digits, so it never jumps a size as the minute changes.
    const std::string shape = digits_as_zero(when);
    const Font alone = fitting(shape, height_ - head_);
    const bool with_later = fitting(shape, height_ - head_ - ui::line_height(Font::BODY)) == alone;
    big_text(when, alone, theme::INK, with_later);
    std::string list;
    for (size_t i = 1; i < next.size() && i < 3; ++i) {
      if (!list.empty()) list += " · ";
      list += when_text(next[i]->at, now);
    }
    if (!with_later) ui::set_text(later_, "");
    else if (stale_) ui::set_text(later_, plugin_->text("old"));
    else ui::set_text(later_, list.empty() ? std::string() : tessera::fill(plugin_->text("then"), "list", list));
  }

  static std::string digits_as_zero(std::string text) {
    for (char &c : text)
      if (c >= '1' && c <= '9') c = '0';
    return text;
  }

  // The largest of the big faces `text` fits in, `room` high and the card wide; BODY_LARGE when none does.
  Font fitting(const std::string &text, int room) const {
    for (Font f : {Font::VALUE, Font::HEADLINE, Font::TITLE})
      if (ui::text_width(text, f) <= width_ && ui::line_height(f) <= room) return f;
    return Font::BODY_LARGE;
  }

  void big_text(const std::string &text, Font face, theme::Role role, bool with_later = true) {
    ui::set_font(big_, face);
    ui::set_color(big_, role);
    ui::set_text(big_, text);
    const int h = ui::line_height(face);
    lv_obj_set_height(big_, h);
    // Between the head and the line under it (or the card's foot without that line), in the middle.
    const int top = head_, bottom = height_ - (with_later ? ui::line_height(Font::BODY) : 0);
    lv_obj_set_pos(big_, 0, top + std::max(0, (bottom - top - h) / 2));
  }

  void paint_list(const std::vector<const Departure *> &next, const char *why, uint32_t now) {
    for (size_t i = 0; i < rows_.size(); ++i) {
      if (i == 0 && why) {
        place_row(rows_[0], "", plugin_->text(why), "");
        continue;
      }
      if (why || i >= next.size()) {
        place_row(rows_[i], "", "", "");
        continue;
      }
      place_row(rows_[i], next[i]->line, with_delay(*next[i]), when_text(next[i]->at, now));
    }
    // The bottom row says when the times are old, if there is room below the rows.
    const int used = top_ + static_cast<int>(rows_.size()) * (head_ + gap_);
    ui::set_text(note_, stale_ && used + ui::line_height(Font::BODY) <= height_ ? plugin_->text("old") : "");
  }

  // The helper never sends more than this (it caps its merged list), so neither the row objects nor the parsed vectors
  // ever grow past it. Keeping both bounded is what keeps the tile within a fixed, small memory footprint.
  static constexpr size_t MAX_DEPARTURES = 8;

  const tessera::Plugin *plugin_;
  int width_ = 0, height_ = 0, head_ = 0, gap_ = 0, top_ = 0, max_rows_ = 0;
  bool list_ = false, header_ = true, late_ = true, has_entity_ = false, wrong_ = false, stale_ = false;
  bool show_minutes_ = true, show_clock_ = false;
  std::string name_;
  std::vector<Row> rows_;
  lv_obj_t *title_{}, *big_{}, *later_{}, *note_{};
  std::vector<Departure> departures_;
  std::vector<const Departure *> next_;
};

void Abfahrtstable::setup() {
  add_tile("board", [this]() { return new BoardTile(this); });
}

}  // namespace esphome::abfahrtstable
