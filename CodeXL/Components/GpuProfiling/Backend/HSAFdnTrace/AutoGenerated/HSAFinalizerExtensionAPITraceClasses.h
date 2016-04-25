//==============================================================================
// Copyright (c) 2015-2016 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief THIS CODE WAS AUTOGENERATED BY PASSTHROUGHGENERATOR ON 04/14/16
//==============================================================================

#ifndef _HSAFINALIZEREXTENSIONAPITRACECLASSES_H_
#define _HSAFINALIZEREXTENSIONAPITRACECLASSES_H_

#include "../HSAAPIBase.h"

///////////////////////////////////////////////////
/// Class used to trace hsa_ext_program_create
///////////////////////////////////////////////////
class HSA_APITrace_hsa_ext_program_create : public HSAAPIBase
{
public:
    /// Constructor
    HSA_APITrace_hsa_ext_program_create();

    /// Destructor
    ~HSA_APITrace_hsa_ext_program_create();

    /// get return value string
    /// \return string representation of the return value;
    std::string GetRetString();

    /// Returns the API's arguments formatted as strings
    /// \return string representation of the API's arguments
    std::string ToString();

    /// Assigns the API's various parameter values
    /// \param machine_model Parameter passed to hsa_ext_program_create
    /// \param profile Parameter passed to hsa_ext_program_create
    /// \param default_float_rounding_mode Parameter passed to hsa_ext_program_create
    /// \param options Parameter passed to hsa_ext_program_create
    /// \param program Parameter passed to hsa_ext_program_create
    void Create(ULONGLONG ullStartTime,
                ULONGLONG ullEndTime,
                hsa_machine_model_t machine_model,
                hsa_profile_t profile,
                hsa_default_float_rounding_mode_t default_float_rounding_mode,
                const char* options,
                hsa_ext_program_t* program,
                hsa_status_t retVal
               );

private:
    /// Disabled copy constructor
    /// \rhs item being copied
    HSA_APITrace_hsa_ext_program_create(const HSA_APITrace_hsa_ext_program_create& rhs);

    /// Disabled assignment operator
    /// \rhs item being assigned
    HSA_APITrace_hsa_ext_program_create& operator = (const HSA_APITrace_hsa_ext_program_create& rhs);

    hsa_machine_model_t m_machine_model; ///< Parameter passed to hsa_ext_program_create
    hsa_profile_t m_profile; ///< Parameter passed to hsa_ext_program_create
    hsa_default_float_rounding_mode_t m_default_float_rounding_mode; ///< Parameter passed to hsa_ext_program_create
    const char* m_options; ///< Parameter passed to hsa_ext_program_create
    std::string m_optionsVal; ///< Member to hold value passed to hsa_ext_program_create in options parameter
    hsa_ext_program_t* m_program; ///< Parameter passed to hsa_ext_program_create
    hsa_ext_program_t m_programVal; ///< Member to hold value passed to hsa_ext_program_create in program parameter
    hsa_status_t m_retVal; ///< Parameter passed to hsa_ext_program_create
};

///////////////////////////////////////////////////
/// Class used to trace hsa_ext_program_destroy
///////////////////////////////////////////////////
class HSA_APITrace_hsa_ext_program_destroy : public HSAAPIBase
{
public:
    /// Constructor
    HSA_APITrace_hsa_ext_program_destroy();

    /// Destructor
    ~HSA_APITrace_hsa_ext_program_destroy();

    /// get return value string
    /// \return string representation of the return value;
    std::string GetRetString();

    /// Returns the API's arguments formatted as strings
    /// \return string representation of the API's arguments
    std::string ToString();

    /// Assigns the API's various parameter values
    /// \param program Parameter passed to hsa_ext_program_destroy
    void Create(ULONGLONG ullStartTime,
                ULONGLONG ullEndTime,
                hsa_ext_program_t program,
                hsa_status_t retVal
               );

private:
    /// Disabled copy constructor
    /// \rhs item being copied
    HSA_APITrace_hsa_ext_program_destroy(const HSA_APITrace_hsa_ext_program_destroy& rhs);

    /// Disabled assignment operator
    /// \rhs item being assigned
    HSA_APITrace_hsa_ext_program_destroy& operator = (const HSA_APITrace_hsa_ext_program_destroy& rhs);

    hsa_ext_program_t m_program; ///< Parameter passed to hsa_ext_program_destroy
    hsa_status_t m_retVal; ///< Parameter passed to hsa_ext_program_destroy
};

///////////////////////////////////////////////////
/// Class used to trace hsa_ext_program_add_module
///////////////////////////////////////////////////
class HSA_APITrace_hsa_ext_program_add_module : public HSAAPIBase
{
public:
    /// Constructor
    HSA_APITrace_hsa_ext_program_add_module();

    /// Destructor
    ~HSA_APITrace_hsa_ext_program_add_module();

    /// get return value string
    /// \return string representation of the return value;
    std::string GetRetString();

    /// Returns the API's arguments formatted as strings
    /// \return string representation of the API's arguments
    std::string ToString();

    /// Assigns the API's various parameter values
    /// \param program Parameter passed to hsa_ext_program_add_module
    /// \param module Parameter passed to hsa_ext_program_add_module
    void Create(ULONGLONG ullStartTime,
                ULONGLONG ullEndTime,
                hsa_ext_program_t program,
                hsa_ext_module_t module,
                hsa_status_t retVal
               );

private:
    /// Disabled copy constructor
    /// \rhs item being copied
    HSA_APITrace_hsa_ext_program_add_module(const HSA_APITrace_hsa_ext_program_add_module& rhs);

    /// Disabled assignment operator
    /// \rhs item being assigned
    HSA_APITrace_hsa_ext_program_add_module& operator = (const HSA_APITrace_hsa_ext_program_add_module& rhs);

    hsa_ext_program_t m_program; ///< Parameter passed to hsa_ext_program_add_module
    hsa_ext_module_t m_module; ///< Parameter passed to hsa_ext_program_add_module
    hsa_status_t m_retVal; ///< Parameter passed to hsa_ext_program_add_module
};

///////////////////////////////////////////////////
/// Class used to trace hsa_ext_program_iterate_modules
///////////////////////////////////////////////////
class HSA_APITrace_hsa_ext_program_iterate_modules : public HSAAPIBase
{
public:
    /// Constructor
    HSA_APITrace_hsa_ext_program_iterate_modules();

    /// Destructor
    ~HSA_APITrace_hsa_ext_program_iterate_modules();

    /// get return value string
    /// \return string representation of the return value;
    std::string GetRetString();

    /// Returns the API's arguments formatted as strings
    /// \return string representation of the API's arguments
    std::string ToString();

    /// Assigns the API's various parameter values
    /// \param program Parameter passed to hsa_ext_program_iterate_modules
    /// \param callback Parameter passed to hsa_ext_program_iterate_modules
    /// \param data Parameter passed to hsa_ext_program_iterate_modules
    void Create(ULONGLONG ullStartTime,
                ULONGLONG ullEndTime,
                hsa_ext_program_t program,
                hsa_status_t (*callback)(hsa_ext_program_t program, hsa_ext_module_t module,
                                         void* data),
                void* data,
                hsa_status_t retVal
               );

private:
    /// Disabled copy constructor
    /// \rhs item being copied
    HSA_APITrace_hsa_ext_program_iterate_modules(const HSA_APITrace_hsa_ext_program_iterate_modules& rhs);

    /// Disabled assignment operator
    /// \rhs item being assigned
    HSA_APITrace_hsa_ext_program_iterate_modules& operator = (const HSA_APITrace_hsa_ext_program_iterate_modules& rhs);

    hsa_ext_program_t m_program; ///< Parameter passed to hsa_ext_program_iterate_modules
    hsa_status_t (*m_callback)(hsa_ext_program_t program, hsa_ext_module_t module,
                               void* data); ///< Parameter passed to hsa_ext_program_iterate_modules
    void* m_data; ///< Parameter passed to hsa_ext_program_iterate_modules
    hsa_status_t m_retVal; ///< Parameter passed to hsa_ext_program_iterate_modules
};

///////////////////////////////////////////////////
/// Class used to trace hsa_ext_program_get_info
///////////////////////////////////////////////////
class HSA_APITrace_hsa_ext_program_get_info : public HSAAPIBase
{
public:
    /// Constructor
    HSA_APITrace_hsa_ext_program_get_info();

    /// Destructor
    ~HSA_APITrace_hsa_ext_program_get_info();

    /// get return value string
    /// \return string representation of the return value;
    std::string GetRetString();

    /// Returns the API's arguments formatted as strings
    /// \return string representation of the API's arguments
    std::string ToString();

    /// Assigns the API's various parameter values
    /// \param program Parameter passed to hsa_ext_program_get_info
    /// \param attribute Parameter passed to hsa_ext_program_get_info
    /// \param value Parameter passed to hsa_ext_program_get_info
    void Create(ULONGLONG ullStartTime,
                ULONGLONG ullEndTime,
                hsa_ext_program_t program,
                hsa_ext_program_info_t attribute,
                void* value,
                hsa_status_t retVal
               );

private:
    /// Disabled copy constructor
    /// \rhs item being copied
    HSA_APITrace_hsa_ext_program_get_info(const HSA_APITrace_hsa_ext_program_get_info& rhs);

    /// Disabled assignment operator
    /// \rhs item being assigned
    HSA_APITrace_hsa_ext_program_get_info& operator = (const HSA_APITrace_hsa_ext_program_get_info& rhs);

    hsa_ext_program_t m_program; ///< Parameter passed to hsa_ext_program_get_info
    hsa_ext_program_info_t m_attribute; ///< Parameter passed to hsa_ext_program_get_info
    void* m_value; ///< Parameter passed to hsa_ext_program_get_info
    hsa_status_t m_retVal; ///< Parameter passed to hsa_ext_program_get_info
};

///////////////////////////////////////////////////
/// Class used to trace hsa_ext_program_finalize
///////////////////////////////////////////////////
class HSA_APITrace_hsa_ext_program_finalize : public HSAAPIBase
{
public:
    /// Constructor
    HSA_APITrace_hsa_ext_program_finalize();

    /// Destructor
    ~HSA_APITrace_hsa_ext_program_finalize();

    /// get return value string
    /// \return string representation of the return value;
    std::string GetRetString();

    /// Returns the API's arguments formatted as strings
    /// \return string representation of the API's arguments
    std::string ToString();

    /// Assigns the API's various parameter values
    /// \param program Parameter passed to hsa_ext_program_finalize
    /// \param isa Parameter passed to hsa_ext_program_finalize
    /// \param call_convention Parameter passed to hsa_ext_program_finalize
    /// \param control_directives Parameter passed to hsa_ext_program_finalize
    /// \param options Parameter passed to hsa_ext_program_finalize
    /// \param code_object_type Parameter passed to hsa_ext_program_finalize
    /// \param code_object Parameter passed to hsa_ext_program_finalize
    void Create(ULONGLONG ullStartTime,
                ULONGLONG ullEndTime,
                hsa_ext_program_t program,
                hsa_isa_t isa,
                int32_t call_convention,
                hsa_ext_control_directives_t control_directives,
                const char* options,
                hsa_code_object_type_t code_object_type,
                hsa_code_object_t* code_object,
                hsa_status_t retVal
               );

private:
    /// Disabled copy constructor
    /// \rhs item being copied
    HSA_APITrace_hsa_ext_program_finalize(const HSA_APITrace_hsa_ext_program_finalize& rhs);

    /// Disabled assignment operator
    /// \rhs item being assigned
    HSA_APITrace_hsa_ext_program_finalize& operator = (const HSA_APITrace_hsa_ext_program_finalize& rhs);

    hsa_ext_program_t m_program; ///< Parameter passed to hsa_ext_program_finalize
    hsa_isa_t m_isa; ///< Parameter passed to hsa_ext_program_finalize
    int32_t m_call_convention; ///< Parameter passed to hsa_ext_program_finalize
    hsa_ext_control_directives_t m_control_directives; ///< Parameter passed to hsa_ext_program_finalize
    const char* m_options; ///< Parameter passed to hsa_ext_program_finalize
    std::string m_optionsVal; ///< Member to hold value passed to hsa_ext_program_finalize in options parameter
    hsa_code_object_type_t m_code_object_type; ///< Parameter passed to hsa_ext_program_finalize
    hsa_code_object_t* m_code_object; ///< Parameter passed to hsa_ext_program_finalize
    hsa_code_object_t m_code_objectVal; ///< Member to hold value passed to hsa_ext_program_finalize in code_object parameter
    hsa_status_t m_retVal; ///< Parameter passed to hsa_ext_program_finalize
};



#endif // _HSAFINALIZEREXTENSIONAPITRACECLASSES_H_
