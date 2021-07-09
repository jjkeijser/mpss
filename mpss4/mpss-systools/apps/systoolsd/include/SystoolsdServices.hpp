#ifndef SYSTOOLS_SYSTOOLSD_SERVICES_HPP_
#define SYSTOOLS_SYSTOOLSD_SERVICES_HPP_

#include <memory>

#include "info/KernelInfo.hpp"
#include "smbios/SmBiosInfo.hpp"

#include "I2cBase.hpp"
#include "PThresh.hpp"
#include "Syscfg.hpp"
#include "TurboSettings.hpp"

// Empty services implementation
class Services
{
public:
    using ptr = std::unique_ptr<Services>;

    Services() { }
    virtual ~Services(){ }
    virtual I2cBase                 *get_i2c_srv() const;
    virtual SmBiosInterface         *get_smbios_srv() const;
    virtual PThreshInterface        *get_pthresh_srv() const;
    virtual TurboSettingsInterface  *get_turbo_srv() const;
    virtual SyscfgInterface         *get_syscfg_srv() const;
    virtual KernelInterface         *get_kernel_srv() const;


private:
    Services &operator=(const Services&) = delete;
    Services(const Services&) = delete;

protected:
    std::unique_ptr<I2cBase>                m_i2c;
    std::unique_ptr<SmBiosInterface>        m_smbios;
    std::unique_ptr<PThreshInterface>       m_pthresh;
    std::unique_ptr<TurboSettingsInterface> m_turbo;
    std::unique_ptr<SyscfgInterface>        m_syscfg;
    std::unique_ptr<KernelInterface>        m_kernel;
};

class SystoolsdServices : public Services
{
public:
    SystoolsdServices();
    SystoolsdServices(std::unique_ptr<I2cBase> i2c,
                      std::unique_ptr<SmBiosInterface> smbios,
                      std::unique_ptr<PThreshInterface> pthresh,
                      std::unique_ptr<TurboSettingsInterface> turbo,
                      std::unique_ptr<SyscfgInterface> syscfg,
                      std::unique_ptr<KernelInterface> kernel);
    virtual ~SystoolsdServices(){ }
};

#endif // SYSTOOLS_SYSTOOLSD_SERVICES_HPP_
