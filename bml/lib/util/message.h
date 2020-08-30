#ifndef COMPILERS_BML_LIB_ERRMSG_H_
#define COMPILERS_BML_LIB_ERRMSG_H_
#include <string>
namespace util::error {

//Assumption: just one source at a time is considered
class message {
 protected:
  //virtual std::string to_string() const = 0;
  virtual std::string_view get_msgtype() const = 0;
  virtual std::string_view get_msgstyle() const;

  virtual std::string_view get_code_token() const = 0; // the point IN CODE to be displayed (if any)
  virtual void print_content(std::ostream&) const = 0;
 public:
  virtual void print(std::ostream&,std::string_view file,std::string_view filename) const;
  //static std::pair<std::string,std::string> context(std::string_view tk);
};

//std::ostream& operator<< (std::ostream& os, const message& em);

//error, warnings, both can be followed by notes, and preceeded by locators
// locators not an message
namespace style {

class error : public virtual message {
  virtual std::string_view get_msgtype() const;
  virtual std::string_view get_msgstyle() const;
};

class note : public virtual message {
  virtual std::string_view get_msgtype() const;
  virtual std::string_view get_msgstyle() const;
};

class warning : public virtual message {
  virtual std::string_view get_msgtype() const;
  virtual std::string_view get_msgstyle() const;
};

}

template<typename Style>
class simple : public virtual message, public virtual Style {
 public:
  std::string_view token, msg;
  virtual std::string_view get_code_token() const { return token; }
  virtual void print_content(std::ostream &os) const { os << msg; }
};


//what if this is a template of error|warning|note ?

template<typename Style>
class report_token : public virtual message, public virtual Style {
 public:
  std::string_view token;
  std::string msg_front,msg_back;
  virtual std::string_view get_code_token() const {return token;}
  virtual void print_content(std::ostream& os) const {
    constexpr std::string_view bold_style="\e[1m";
    constexpr std::string_view clear_style="\e[0m";
    os << msg_front <<std::string_view(" '")<<bold_style<<token<<clear_style<<std::string_view("' ")<<msg_back;
  }
  report_token(std::string_view f,std::string_view t,std::string_view b) : msg_front(f),token(t),msg_back(b) {}
};
/*
template<typename BaseMsg>
class report_token_string : public BaseMsg {
 public:
  std::string_view token,msg_front,msg_back;
  std::string quote;
  virtual std::string_view get_code_token() const {return token;}
  virtual void print_content(std::ostream& os) const {
    constexpr std::string_view bold_style="\e[1m";
    constexpr std::string_view clear_style="\e[0m";
    os << msg_front <<std::string_view(" '")<<bold_style<<quote<<clear_style<<std::string_view("' ")<<msg_back;
  }
  report_token_string(std::string_view t,std::string_view f,std::string_view q,std::string_view b) : msg_front(f),token(t),msg_back(b),quote(q) {}
};
*/
typedef report_token<style::error> report_token_error;



}
#endif //COMPILERS_BML_LIB_ERRMSG_H_
