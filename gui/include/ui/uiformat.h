#pragma once
// Pure text helpers for the UI (no widgets), so they are unit-testable.
#include <QString>

namespace UiFormat {

QString count(qint64 n);                              // 12345 -> "12,345"
int percent(qint64 done, qint64 total);               // clamped 0..100; -1 when total <= 0
QString progressSummary(qint64 done, qint64 total);   // "12 of 345 files (3%)"; empty if total <= 0

}  // namespace UiFormat
