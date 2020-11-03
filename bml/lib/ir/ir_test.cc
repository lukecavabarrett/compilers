#include <ir/lang.h>
#include <gtest/gtest.h>

namespace ir::lang {

TEST(Lang, Syntax) {
  var x, y, z;
  mov(x, y[1]);
}

TEST(Lang, Function) {
  function f;
  var arg_browser;
  f.push_back(mov(arg_browser, argv_var));
  var x, y, ans;
  f << mov(x, arg_browser[2])
    << mov(arg_browser, arg_browser[3])
    << mov(y, arg_browser[2]);
  //f.push_back()
}

}