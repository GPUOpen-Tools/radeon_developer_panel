// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Unit tests for the SPM counter file parser (PANEL-2349).
///
/// Regression tests for two bugs in ParseSpmCounterContent:
///
///   Bug 1 (duplicate bare counter push):
///     A counter line with no '=' was pushed into requested_spm_counters twice:
///     once inside the dedup-guarded branch and again unconditionally after the
///     if/else block.  The fix moves the unconditional push inside the else branch
///     so that bare counters are never duplicated.
///
///   Bug 2 (off-by-one '=' search):
///     line[idx++] post-increments idx before the section-header test, so the
///     subsequent find('=', idx) searched from idx+1.  For a line that starts with
///     '=' this caused the '=' to be skipped.  The fix resets the search start to
///     the original non-whitespace position.
