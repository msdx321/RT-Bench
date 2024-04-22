#! /usr/bin/env bash
# This script will generate the documentation for all the branches of the repository in the html folder.

# create folder for the documentation
CURRENT_CONF=$(realpath .)
DOCS_FOLDER=$(realpath ../)"/html/branch"
mkdir -p "$DOCS_FOLDER" || exit 255

make_docs() {
	echo "Building documentation for $branch"
	folder_name=$(echo "$branch" | sed 's/\//-/g' | sed 's/+/plus/g')
	# branches with docs already built are skipped
	if [ -d "$DOCS_FOLDER/$folder_name" ]; then
		echo "Documentation for $branch already exists, skipping..."
		return
	fi
	# we need a fresh copy of the repository to avoid conflicts while checking out branches
	git clone -b "$branch" --depth 1 --shallow-submodules https://gitlab.com/rt-bench/rt-bench.git "$DOCS_FOLDER"/../rt-bench-tmp || exit 255
	cd "$DOCS_FOLDER"/../rt-bench-tmp || exit 255
	DOCS_ONLY=1 make setup || exit 255
	cd docs || exit 255
	# make sure all branches are built with the current doxygen configuration
	cp -r "$CURRENT_CONF"/../Makefile . || exit 255
	cp -r "$CURRENT_CONF"/* "./conf" || exit 255
	# change the project number to the branch name
	sed -i "s|PROJECT_NUMBER\s*=.*|PROJECT_NUMBER=$branch|" conf/Doxyfile || exit 255
	# remove the warnings
	sed -i -e "s|WARNINGS\s*=.*|WARNINGS=NO|" -e "s|WARN_AS_ERROR\s=|WARN_AS_ERROR=NO|" conf/Doxyfile || exit 255
	CURRENT_BRANCH="$branch" make html
	if [ -d html/"$folder_name" ]; then
		mv html/"$folder_name" "$DOCS_FOLDER" || exit 255
	else
		mkdir -p "$DOCS_FOLDER/$folder_name" || exit 255
		mv html/* "$DOCS_FOLDER/$folder_name" || exit 255
	fi
	cd "$DOCS_FOLDER" || exit 255
	rm -rf "$DOCS_FOLDER"/../rt-bench-tmp
}

# build docs for all branches
git for-each-ref --format='%(refname:short)' refs/tags refs/remotes/origin | while read -r branch; do
	# remove the origin/ prefix
	branch=$(echo "$branch" | sed 's/origin\///')
	# skip the HEAD reference
	if [ "$branch" == "HEAD" ]; then
		continue
	fi
	make_docs
done
cd "$DOCS_FOLDER/.." || exit 255
# create a index.html file that redirects to the main branch
echo '<!DOCTYPE html> <html lang="en"> <head> <meta http-equiv="refresh" content="0; url=./branch/main/index.html" /> </head> </html>' >./index.html || exit 255
if [ ! -d "$DOCS_FOLDER/main" ]; then
	echo "Error: Documentation for the main branch does not exist!"
	exit 255
fi
