
echo "Killing all dfs processes:"
ps -aux | grep dfs | grep -oP "(?<=^codespa\+\s\s)\d+"
ps -aux | grep dfs | grep -oP "(?<=^codespa\+\s)\d+"
kill $(ps -aux | grep dfs | grep -oP "(?<=^codespa\+\s\s)\d+")
kill $(ps -aux | grep dfs | grep -oP "(?<=^codespa\+\s)\d+")
echo "Killed all dfs processes:"
ps -aux | grep dfs | grep -oP "(?<=^codespa\+\s\s)\d+"
ps -aux | grep dfs | grep -oP "(?<=^codespa\+\s)\d+"
