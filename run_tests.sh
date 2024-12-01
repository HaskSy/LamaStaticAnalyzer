#!/usr/bin/env bash

if command -v readlink > /dev/null; then
  SCRIPT_DIR=$(dirname "$(readlink -f "$0")")
else
  SCRIPT_DIR=$(dirname "$(cd "$(dirname "$0")" && pwd)")
fi

if [ -z "$SCRIPT_DIR" ]; then
  echo "Error: Failed to determine the script directory." >&2
  exit 1
fi

pushd $SCRIPT_DIR 2>&1 1>/dev/null || exit

mkdir -p tests
pushd tests 2>&1 1>/dev/null || exit

LAMA_INTERPRETER="../build/LamaInterpreter"
LAMA_PATHS=(
    "../Lama/regression"
    "../Lama/regression/deep-expressions"
    "../Lama/regression/expressions"
)

BLACKLIST=(
    "../Lama/regression/test111.lama"
)

declare -A failed_tests

for LAMA_PATH in "${LAMA_PATHS[@]}"; do
    for file in "$LAMA_PATH"/*.lama; do
        # Skip files in the blacklist
        if [[ " ${BLACKLIST[*]} " =~ " $file " ]]; then
            echo "Skipping blacklisted file: $file"
            continue
        fi

        baseName=$(basename "$file" .lama)


        lamac -b "$file"
        output=$($LAMA_INTERPRETER "$baseName.bc" < "$LAMA_PATH/$baseName.input")

        if diff <(echo "$output") "$LAMA_PATH/orig/$baseName.log" > /dev/null; then
            echo "Output for $baseName matches"
        else
            echo "Output for $baseName does not match!"
            echo "Expected:"
            cat "$LAMA_PATH/orig/$baseName.log" 
            echo "But got:"
            echo "$output"
            echo "lama file:"
            cat "$file"
            failed_tests["$file"]="$output"
        fi
    done
done

LAMA_PERF_PATH="../Lama/performance"
for file in "$LAMA_PERF_PATH"/*.lama; do
    baseName=$(basename "$file" .lama)
    echo "-------------------------------"
    echo "Testing file: $file"
    echo "-------------------------------"
    lamac "$file"
    echo "Binary time:"
    time "./$baseName"
    echo "-------------------------------"
    echo "Original interpreter time with compilation:"
    touch dummy
    time lamac -s "$file" < dummy
    rm dummy
    echo "-------------------------------"
    echo "Our interpreter time with compilation:"
    time (
        lamac -b "$file"
        $LAMA_INTERPRETER "$baseName.bc"
    )
    echo "-------------------------------"
    echo "Our interpreter time without compilation:"
    time $($LAMA_INTERPRETER "$baseName.bc")
    echo "-------------------------------"
done

popd 2>&1 1>/dev/null || exit
popd 2>&1 1>/dev/null || exit

if [ ${#failed_tests[@]} -gt 0 ]; then
    echo "Summary of failed tests:"
    for test in "${!failed_tests[@]}"; do
        echo "Test: $test"
        echo "Output:"
        echo "${failed_tests[$test]}"
        echo "----"
    done
else
    echo "All tests passed!"
fi
