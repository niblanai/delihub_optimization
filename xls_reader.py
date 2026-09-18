#!/usr/bin/env python3
"""
xls_reader.py — Extract product name + quantity from ABA Market XLS invoices.
"""

import sys
import json

# Force UTF-8 output on Windows (default is cp1252 which can't encode Arabic)
if sys.platform == "win32":
    import io
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

def extract(path):
    try:
        import xlrd
    except ImportError:
        sys.stderr.write("xlrd not installed\n")
        sys.exit(1)

    try:
        wb = xlrd.open_workbook(path)
    except Exception as e:
        sys.stderr.write(f"Cannot open file: {e}\n")
        sys.exit(1)

    ws = wb.sheet_by_index(0)
    items = []

    # Products start at row index 5 (row 6 in 1-based)
    START_ROW = 5

    for r in range(START_ROW, ws.nrows):
        # Read all cells up to col G (index 6) safely
        def cell(c):
            if c < ws.ncols:
                return str(ws.cell_value(r, c)).strip()
            return ''

        col_c = cell(2)   # C: quantity
        col_d = cell(3)   # D: product name (Arabic, RTL)

        name = col_d.strip()

        # Skip empty rows
        if not name:
            continue

        # Stop at footer rows:
        # 1. Name contains "فقط" (Arabic for "only") = total-amount text row
        # 2. Col C has no valid positive number = not a product row (e.g. total row)
        # 3. Name has no Arabic characters at all
        has_arabic = any('\u0600' <= ch <= '\u06FF' for ch in name)
        if not has_arabic:
            continue
        if 'فقط' in name:
            break

        # Parse quantity from col C — STOP if no valid quantity
        try:
            qty = float(col_c)
            if qty <= 0:
                # Row has no qty → likely a footer/total row → stop
                break
        except (ValueError, TypeError):
            # Col C is empty or non-numeric → stop (footer reached)
            break

        items.append({'name': name, 'qty': qty})

    return items


if __name__ == '__main__':
    if len(sys.argv) < 2:
        sys.stderr.write("Usage: xls_reader.py <file.xls>\n")
        sys.exit(1)

    result = extract(sys.argv[1])
    # Output UTF-8 JSON to stdout
    print(json.dumps(result, ensure_ascii=False))
