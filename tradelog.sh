#!/bin/sh
# Project #1
# Brno University of Technology | Faculty of Information Technology
# Course: Operation Systems | Summer semester 2021
# Author: Denis Karev (xkarev00@stud.fit.vutbr.cz)
# This project should not be used for non-educational purposes.

export POSIXLY_CORRECT=yes
export LC_NUMERIC=en_US.UTF-8

# Shows a help message
show_help ()
{
  echo "usage: tradelog [-h|--help] [FILTER] [COMMAND] [LOG [LOG2 [...]]"
  echo ""
	echo "Available commands:"
	echo "list-tick   list of tickers in the trade log"
	echo "profit      total profit from the closed positions"
	echo "pos         value of open positions, ordered by value"
	echo "last-price  last-known price for every ticker"
	echo "hist-ord    transaction amount histogram by ticker"
	echo "graph-pos   open position values graph"
  echo ""
	echo "FILTER can be a combination of:"
	echo "-a DATETIME includes records after the specified date (excluding it). DATETIME format is YYYY-MM-DD HH:MM:SS."
	echo "-b DATETIME includes records before the specified date (excluding it)."
	echo "-t TICKER   only records of the specified ticker are included."
	echo "-w WIDTH    for graphs sets the width (length) of the longest string."
}

INPUT=""

AFTER="0001-01-01 00:00:00"
BEFORE="9999-12-31 23:59:59"
TICKERS=""
WIDTH="0"

COMMAND=""

while [ "$#" -gt 0 ]; do
  case "$1" in
  # Help message
  -h | --help)
    show_help
    exit 0
    ;;
  # Commands
  list-tick | profit | pos | last-price | hist-ord | graph-pos)
    COMMAND="$1"
    shift
    ;;
  # Filters
  -a)
    AFTER="$2"
    shift
    shift
    ;;
  -b)
    BEFORE="$2"
    shift
    shift
    ;;
  -t)
    TICKERS="$2;$TICKERS"
    shift
    shift
    ;;
  -w)
    if [ $WIDTH -ne 0 ]; then
        echo "Error: can't use -w option more then once"
        exit 1
    fi
    if [ $2 -le 0 ]; then
        echo "Error: -w parameter must be greater then zero"
        exit 1
    fi

    WIDTH="$2"
    shift
    shift
    ;;
  # Log files
  *.gz)
    if [ -f "$1" ]; then
      INPUT="$INPUT$(gzip -d -c $1)\n"
      shift
    else
      echo "Error: $1 is an invalid operation, filter or file name. Use tradelog [-h|--help] to get help."
      exit 1
    fi
    ;;
  *)
    if [ -f "$1" ]; then
      INPUT="$INPUT$(cat $1)\n"
      shift
    else
      echo "Error: $1 is an invalid operation, filter or file name. Use tradelog [-h|--help] to get help."
      exit 1
    fi
    ;;
  esac
done

# If no logs were specified, script would read from stdin
if [ -z "$INPUT" ]; then
    while read -r NEXT_LINE; do
      INPUT="$INPUT$NEXT_LINE\n"
    done
fi

# Filtering input
FILTERED=$(echo "$INPUT" | awk --posix -v after="$AFTER" -v before="$BEFORE" -v i_tickers="$TICKERS" -F\; '
BEGIN {
  split(i_tickers,tickers,";")
}
{
  if ($1 > after && $1 < before) {
    if (i_tickers != "") {
      for (ticker in tickers) {
        if ($2 == tickers[ticker]) {
          print $0
          break
        }
      }
    }
    else {
      print $0
    }
  }
}')

# Performing commands

if [ "$COMMAND" = "" ]; then                # Command is empty => print filtered content
   echo "$FILTERED"
elif [ "$COMMAND" = "list-tick" ]; then     # Cycle through the filtered content and find all unique ticker entries
  echo "$FILTERED" | awk --posix -F\; '{ print $2 }' | sort -u
elif [ "$COMMAND" = "profit" ]; then        # Profit is calculated as a sum of sell transaction values minus sum of buy transaction values
  echo "$FILTERED" | awk --posix -F\; '
  BEGIN { sum = 0 }
  {
    if ($3 == "sell")
      sum += $4 * $6
    else
      sum -= $4 * $6
  }
  END { printf "%.2f\n", sum }
  '
elif [ "$COMMAND" = "pos" ]; then           # Prints total price of stocks, held at the current moment. Last-known price of the stock is used
  if [ -z "$FILTERED" ]; then
      echo "You are trying to run $COMMAND on empty content. Check your input and filters and try again"
      exit 1
  fi
  echo "$FILTERED" | awk --posix -F\; '
  {
    if ($3 == "buy")
      hold[$2] += $6
    else
      hold[$2] -= $6
    values[$2] = hold[$2] * $4
  }
  END {
    # finds the length of the longest value, as we want some pretty formatting
    longest = 0
    for (ticker in values) {
      fstr = sprintf("%.2f", values[ticker])

      if (length(fstr) > longest)
        longest = length(fstr)
    }

    for (ticker in values) {
      printf "%-10s: %*.2f\n", ticker, longest, values[ticker]
    }
  }' | sort -n -r -k 3
elif [ "$COMMAND" = "last-price" ]; then    # Prints last known price of every stock.
  if [ -z "$FILTERED" ]; then
      echo "You are trying to run $COMMAND on empty content. Check your input and filters and try again"
      exit 1
  fi
  echo "$FILTERED" | awk --posix -F\; '
  {
    last_price[$2] = $4
  }
  END {
    # finds the length of the longest value, as we want some pretty formatting
    longest = 0
    for (ticker in last_price) {
      fstr = sprintf("%.2f", last_price[ticker])

      if (length(fstr) > longest)
        longest = length(fstr)
    }

    for (ticker in last_price) {
      printf "%-10s: %*.2f\n", ticker, longest, last_price[ticker]
    }
  }' | sort
elif [ "$COMMAND" = "hist-ord" ]; then      # Draws a marvelous histogram, based on the amount of transactions for each ticker
  if [ -z "$FILTERED" ]; then
      echo "You are trying to run $COMMAND on empty content. Check your input and filters and try again"
      exit 1
  fi
  echo "$FILTERED" | awk --posix -v width="$WIDTH" -F\; '
  { ++transactions[$2] }
  END {
    if (width == 0) {
      for (ticker in transactions) {
        graph = ""
        for (i = 1; i <= transactions[ticker]; ++i) {
          graph = sprintf("%s#", graph)
        }
        printf "%-10s: %s\n", ticker, graph
      }
    } else {
      max = 0
      for (ticker in transactions) {
        if (transactions[ticker] > max)
          max = transactions[ticker]
      }
      unit = max / width

      # Output
      for (ticker in transactions) {
        graph = ""
        units = int(transactions[ticker] / unit)
        for (i = 1; i <= units; ++i) {
          graph = sprintf("%s#", graph)
        }
        printf "%-10s: %s\n", ticker, graph
      }
    }
  }' | sort
elif [ "$COMMAND" = "graph-pos" ]; then     # Draws a fancy graph, based on the total price of every stock, held at the current moment. # for positive, ! for negative
  if [ -z "$FILTERED" ]; then
      echo "You are trying to run $COMMAND on empty content. Check your input and filters and try again"
      exit 1
  fi
  echo "$FILTERED" | awk --posix -v width="$WIDTH" -F\; '
  function abs(x) {
    return x >= 0 ? x : -x
  }
  {
    if ($3 == "buy")
      hold[$2] += $6
    else
      hold[$2] -= $6
    values[$2] = hold[$2] * $4
  }
  END {
    unit = 1000
    if (width != 0) {
      max = 0
      for (ticker in values) {
        if (abs(values[ticker]) > max) {
          max = abs(values[ticker])
        }
      }

      unit = max / width
    }

    for (ticker in values) {
      graph = ""
      symbol = values[ticker] > 0 ? "#" : "!"
      units = abs(int(values[ticker] / unit))
      for (i = 1; i <= units; ++i) {
        graph = sprintf("%s%c", graph, symbol)
      }
      printf "%-10s: %s\n", ticker, graph
    }
  }' | sort
fi
