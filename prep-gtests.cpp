
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <vector>

#include "line-reader.hpp"
#include "ctre.hpp"

using namespace std::string_literals;

// parse log file
void parselogfile(jaw::reader &reader,
                  std::map<std::string, int> &results)
{
  // for each line
  while (reader.hasline())
  {
    // get a line
    auto line = reader.getline();

    // try and parse out test suite execution time
    static constexpr auto testsuite =
        ctll::fixed_string{"] [0-9]+ test from ([a-zA-Z]+).*[(]([0-9]+) ms total"};
    auto matched = ctre::search<testsuite>(line);

    if (matched)
    {
      // add to results
      // std::cout << "#: " << matched.count() << " matches." << std::endl;
      // std::cout << "1: " << matched.get<1>() << std::endl;
      // std::cout << "2: " << matched.get<2>() << std::endl;
      results.insert(std::pair(std::string(matched.get<1>()),
                               matched.get<2>().to_number<int>()));
    }
  }
}

int main(int argc, char **argv)
{
  // vars
  std::vector<std::pair<std::string, std::string>> runs;
  std::map<std::string, std::map<std::string, int>> runresults;
  std::map<std::string, int> runtotal;
  std::map<std::string, int> testsuitemin;
  std::map<std::string, int> testsuitemax;
  std::map<std::string, int> testsuitepct;
  std::vector<std::pair<std::string, int>> sortedtestsuitemax;
  std::string longestsuitename;
  auto firstcolumn{0ul};

  // scan over command-line args
  for (auto i{1}; i < argc; ++i)
  {
    // get filename
    std::string filename(argv[i]);
    std::string runname(filename);

    // get optional title
    if (i+1<argc && "--as"s == argv[i+1])
    {
      if (i+2<argc) {
        // get --as title, and bump i by 2
        runname = std::string(argv[i+=2]);
      } else {
        std::cerr << "Expected optional name after reading --as." << std::endl;
        goto usage;
      }
    }

    // add filename, runname to vector
    runs.push_back(make_pair(filename,runname));
  }

  // need at least one file
  if (!runs.size()) {
    std::cerr << "Please provide at least one file containing " <<
                 "Googletest output." << std::endl;
    goto usage;
  }

  // for each file passed on command-line, with filename or --as 'title'
  for (auto run : runs) {
      // get filename and runname
      auto filename = run.first;
      auto runname = run.second;

      // read results out of file
      jaw::reader reader(filename);
      std::map<std::string, int> results;
      parselogfile(reader, results);
      runresults.insert(make_pair(runname, results));
      
      // for each test suite
      for (auto result : results)
      {
        // track longest test suite name for formatting ... length, suite
        std::string testsuite(result.first);
        longestsuitename = testsuite.length()>longestsuitename.length() ?
                           testsuite : longestsuitename;
        int execution{result.second};

        // for testsuitemin, enter maxint if an entry hasn't been added
        if (testsuitemin.find(testsuite)==testsuitemin.end()) {
          testsuitemin.insert(make_pair(testsuite, 
                                        std::numeric_limits<int>::max()));
        }

        // track min / max suite execution ... min, suite and max, suite
        auto executionmin = std::min(testsuitemin[testsuite],execution);
        testsuitemin[testsuite] = executionmin;
        auto executionmax = std::max(testsuitemax[testsuite],execution);
        testsuitemax[testsuite] = executionmax;
        // calculate as a speedup between slowest and fastest
        auto pct = int(testsuitemax[testsuite]*100.0/testsuitemin[testsuite]-100.0);
        testsuitepct[testsuite] = pct;

        // increment test suite count for each recorded result ... map<run, int>

        // calculate total execution time for each run ... map<run, int>
        runtotal[runname] += execution;
      }
  }

  // Test Suite ... Filename / Run 1 ... Filename / Run 2 ... Speedup/Variation
  firstcolumn = longestsuitename.length();
  std::cout << std::setw(firstcolumn) << "\"Test Suite\"";
  // for rest of table header
  for (auto run : runs) {
      // print run name / title
      std::cout << "\t\"" << run.second << "\"";
  }
  // print speed up / variation column header
  std::cout << "\t" << "\"Speedup Percent / Variation\"" << std::endl;

  // sort test suites high .. low
  std::copy(testsuitemax.begin(), testsuitemax.end(),
            std::back_inserter(sortedtestsuitemax));
  std::sort(sortedtestsuitemax.begin(), sortedtestsuitemax.end(),
            [](std::pair<std::string ,int> a,std::pair<std::string,int> b) {
              return b.second<a.second;
            });

  // for each test suite row
  for (auto suite : sortedtestsuitemax)
  {
    // print test suite name padded to longest
    std::cout << std::setw(firstcolumn) << suite.first;

    // for each run
    for (auto run : runs) {
      // print execution time of current suite
      std::cout << "  " << std::setw(12) << runresults[run.second][suite.first];
    }

    // print speedup ... slowest to fastest
    std::cout << "  " << std::setw(12) << testsuitepct[suite.first] << std::endl;
  }

  return 0;

  // print usage instructions
usage:
  std::cerr << "usage: " << argv[0] << " log-file-1 [--as \"named run 1\"] "
            << "log-file-2 [--as \"named run 2\"] etc.\n" << std::endl;
  exit(1);
}