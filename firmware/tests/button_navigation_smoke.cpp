#include <cassert>
#include <cstdint>
#include "button_navigation.hpp"

using Action = ButtonNavigation::Action;

static Action sample(ButtonNavigation& nav, uint32_t time, bool power, bool boot) {
  return nav.update(time, power, boot);
}

int main() {
  {
    ButtonNavigation nav;
    assert(sample(nav, 0, false, false) == Action::None);
    assert(sample(nav, 10, true, false) == Action::None);
    assert(sample(nav, 20, false, false) == Action::None); // bounce
    assert(sample(nav, 50, true, false) == Action::None);
    assert(sample(nav, 80, true, false) == Action::None);
    assert(sample(nav, 90, false, false) == Action::None);
    assert(sample(nav, 120, false, false) == Action::NextPage);
    assert(sample(nav, 150, false, false) == Action::None);
    assert(sample(nav, 180, false, true) == Action::None);
    assert(sample(nav, 210, false, true) == Action::None);
    assert(sample(nav, 240, false, false) == Action::None);
    assert(sample(nav, 270, false, false) == Action::Settings);
  }
  {
    ButtonNavigation nav;
    assert(sample(nav, 0, true, false) == Action::None); // held at boot
    assert(sample(nav, 100, false, false) == Action::None);
    assert(sample(nav, 130, false, false) == Action::None);
    assert(sample(nav, 160, true, false) == Action::None);
    assert(sample(nav, 190, true, false) == Action::None);
    assert(sample(nav, 220, false, false) == Action::None);
    assert(sample(nav, 250, false, false) == Action::NextPage);
  }
  {
    ButtonNavigation nav;
    sample(nav, 0, false, false);
    sample(nav, 100, true, false);
    sample(nav, 130, true, false);
    sample(nav, 160, true, true);
    sample(nav, 190, true, true);
    assert(sample(nav, 600, false, true) == Action::None);
    assert(sample(nav, 630, false, true) == Action::None);
    assert(sample(nav, 660, false, false) == Action::None);
    assert(sample(nav, 690, false, false) == Action::None); // short chord cancels
  }
  {
    ButtonNavigation nav;
    sample(nav, 0, false, false);
    sample(nav, 100, true, true);
    sample(nav, 130, true, true);
    assert(sample(nav, 1129, true, true) == Action::None);
    assert(sample(nav, 1130, true, true) == Action::None); // long chord is ignored
    assert(sample(nav, 1200, false, true) == Action::None);
    assert(sample(nav, 1230, false, true) == Action::None);
    assert(sample(nav, 1300, false, false) == Action::None);
    assert(sample(nav, 1330, false, false) == Action::None); // no restart
    assert(sample(nav, 1360, false, false) == Action::None);
    sample(nav, 1400, true, false);
    sample(nav, 1430, true, false);
    sample(nav, 1460, false, false);
    assert(sample(nav, 1490, false, false) == Action::NextPage);
  }
  {
    ButtonNavigation nav;
    sample(nav, UINT32_MAX - 100, false, false);
    sample(nav, UINT32_MAX - 50, false, true);
    sample(nav, UINT32_MAX - 20, false, true);
    sample(nav, 10, false, false);
    assert(sample(nav, 40, false, false) == Action::Settings);
  }
}
