// test_ui_scale.cpp
//
// SURI: tests for features/ui_scale.* (DPI font scaling, help paging, toast queue).

#include "mini_test.h"
#include <atomic>
#include <thread>
#include "..\vis_milk2\features\ui_scale.h"

TEST(TestUiScale_FontPixels) {
    CHECK(ScaleFontPixels(14, 96) == 14);
    CHECK(ScaleFontPixels(14, 144) == 21);
    CHECK(ScaleFontPixels(14, 192) == 28);
    CHECK(ScaleFontPixels(14, 72) == 14);
}
TEST(TestUiScale_HelpPages) {
    std::vector<std::wstring> p = SplitHelpPages(L"one\nuno\n\f\ntwo\n\f\nthree\n");
    CHECK(p.size() == 3);
    CHECK(p[0] == L"one\nuno"); CHECK(p[1] == L"two"); CHECK(p[2] == L"three");
    CHECK(SplitHelpPages(L"just text\nmore").size() == 1);
    CHECK(SplitHelpPages(L"a\n\f\nb\n\f\n").size() == 2);       // trailing separator: no empty page
    CHECK(SplitHelpPages(L"a\r\n\f\r\nb").size() == 2);         // CRLF
    CHECK(SplitHelpPages(L"").size() == 1);
}
TEST(TestUiScale_ToastQueue) {
    ToastQueue q;
    q.Push(L"a", 0.0f, 2.0f); q.Push(L"b", 0.1f, 2.0f); q.Push(L"c", 0.2f, 2.0f); q.Push(L"d", 0.3f, 2.0f);
    std::vector<Toast> v = q.Visible(0.5f);
    CHECK(v.size() == 3);
    CHECK(v[0].text == L"d"); CHECK(v[1].text == L"c"); CHECK(v[2].text == L"b");
    CHECK(q.Visible(2.15f).size() == 2);      // a and b expired
    CHECK(q.Visible(5.0f).size() == 0);
}
TEST(TestUiScale_ToastErrorFlag) {
    ToastQueue q;
    q.Push(L"saved", 0.0f, 2.5f);
    q.Push(L"broken shader", 0.1f, 6.0f, true);
    std::vector<Toast> v = q.Visible(0.2f);
    CHECK(v.size() == 2);
    CHECK(v[0].isError); CHECK(!v[1].isError);
}
TEST(TestUiScale_ToastClearByTag) {
    ToastQueue q;
    q.Push(L"a", 0.0f, 5.0f, false, 4); q.Push(L"b", 0.0f, 5.0f, true, 2); q.Push(L"c", 0.0f, 5.0f, false, 4);
    q.Clear(4);
    std::vector<Toast> v = q.Visible(1.0f);
    CHECK(v.size() == 1); CHECK(v[0].text == L"b");
    q.Clear();
    CHECK(q.Visible(1.0f).size() == 0);
}
// SURI: the preset-scan thread pushes / clears while the render thread reads: Visible returns
// copies and the queue locks, so the reader never sees freed memory.
TEST(TestUiScale_ToastQueueThreads) {
    ToastQueue q;
    std::atomic<bool> stop(false);
    std::thread writer([&]() {
        for (int i = 0; i < 20000; i++) {
            q.Push(L"scanning presets, a long enough text to live on the heap", 0.0f, 100.0f, false, 5);
            if (i % 3 == 0) q.Clear(5);
        }
        stop = true;
    });
    size_t total = 0;
    while (!stop) {
        std::vector<Toast> v = q.Visible(1.0f);
        for (const Toast& t : v) total += t.text.size();
        CHECK(v.size() <= ToastQueue::kMaxVisible);
    }
    writer.join();
    (void)total;
}
TEST(TestUiScale_ParseHelpPage) {
    std::wstring title;
    std::vector<HelpRow> r = ParseHelpPage(L"View\n\nF2\tframe rate\n\tlimit\nSome words.\n\nAlt+F4\tquit", &title);
    CHECK(title == L"View");
    CHECK(r.size() == 3);
    CHECK(r[0].key == L"F2"); CHECK(r[0].text == L"frame rate limit");
    CHECK(r[1].key.empty()); CHECK(r[1].text == L"Some words.");
    CHECK(r[2].key == L"Alt+F4"); CHECK(r[2].text == L"quit");
}
