// Copyright (c) 2014-2016, Massachusetts Institute of Technology
//
// This file is part of the Compressed Continuous Computation (C3) toolbox
// Author: Alex A. Gorodetsky 
// Contact: goroda@mit.edu

// All rights reserved.

// Redistribution and use in source and binary forms, with or without modification, 
// are permitted provided that the following conditions are met:

// 1. Redistributions of source code must retain the above copyright notice, 
//    this list of conditions and the following disclaimer.

// 2. Redistributions in binary form must reproduce the above copyright notice, 
//    this list of conditions and the following disclaimer in the documentation 
//    and/or other materials provided with the distribution.

// 3. Neither the name of the copyright holder nor the names of its contributors 
//    may be used to endorse or promote products derived from this software 
//    without specific prior written permission.

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE 
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

//Code

/** \file fapprox.c
 * Provides basic routines for approximating functions
 */

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#include "fapprox.h"

/***********************************************************//**
  Allocate one dimensional approximations
***************************************************************/
struct OneApproxOpts * 
one_approx_opts_alloc(enum function_class fc, void * aopts)
{

    struct OneApproxOpts * app = malloc(sizeof(struct OneApproxOpts));
    if (app == NULL){
        fprintf(stderr,"Cannot allocate OneApproxOpts\n");
        exit(1);
    }
    app->fc = fc;
    app->aopts = aopts;
    return app;
}

/***********************************************************//**
  Free one dimensional approximations
***************************************************************/
void one_approx_opts_free(struct OneApproxOpts * oa)
{
    if (oa != NULL){
        free(oa); oa = NULL;
    }
}

//////////////////////////////////////////////////////
/** \struct MultiApproxOpts
 * \brief Multidimensional approximation arguments
 * \var MultiApproxOpts::dim
 * function dimension
 * \var MultiApproxOpts::aopts
 * function approximation options
 */
struct MultiApproxOpts
{
    size_t dim;
    struct OneApproxOpts ** aopts;
};

/***********************************************************//**
  Allocate multi_approx_opts
  \param[in] dim - dimension
  
  \return approximation options
***************************************************************/
struct MultiApproxOpts * multi_approx_opts_alloc(size_t dim)
{
    struct MultiApproxOpts * fargs;
    if ( NULL == (fargs = malloc(sizeof(struct MultiApproxOpts)))){
        fprintf(stderr, "Cannot allocate space for MultiApproxOpts.\n");
        exit(1);
    }
    fargs->aopts = malloc(dim * sizeof(struct OneApproxOpts *));
    if (fargs->aopts == NULL){
        fprintf(stderr, "Cannot allocate MultiApproxOpts\n");
        exit(1);
    }
    fargs->dim = dim;
    for (size_t ii = 0; ii < dim; ii++){
        fargs->aopts[ii] = NULL;
    }

    return fargs;
}

/***********************************************************//**
    Free memory allocated to MultiApproxOpts (shallow)

    \param[in,out] fargs - function train approximation arguments
***************************************************************/
void multi_approx_opts_free(struct MultiApproxOpts * fargs)
{
    if (fargs != NULL){
        /* for (size_t ii = 0; ii < fargs->dim;ii++){ */
        /*     one_approx_optsfree(fargs->aopts[ii]); */
        /*     fargs->aopts[ii] = NULL; */
        /* } */
        free(fargs->aopts); fargs->aopts = NULL;
        free(fargs); fargs = NULL;
    }
}

/***********************************************************//**
    Set approximation options for a particular dimension
    \param[in,out] fargs - function train approximation arguments
***************************************************************/
void multi_approx_opts_set_dim(struct MultiApproxOpts * fargs,
                               size_t ind,
                               struct OneApproxOpts * opts)
{
    assert (fargs != NULL);
    assert (ind < fargs->dim);
    fargs->aopts[ind] = opts;
}

/***********************************************************//**
    Create the arguments to give to use for approximation
    in the function train. Specifically, legendre polynomials
    for all dimensions

    \param[in,out] mopts 
    \param[in]     opts 

    \return approximation arguments
***************************************************************/
void
multi_approx_opts_set_all_same(struct MultiApproxOpts * mopts,
                               struct OneApproxOpts * opts)
{
    assert(mopts != NULL);
    for (size_t ii = 0; ii < mopts->dim; ii++){
        mopts->aopts[ii] = opts;
    }
}                               

/***********************************************************//**
    Extract the function class to use for the approximation of the
    *dim*-th dimensional functions 

    \param[in] fargs - function train approximation arguments
    \param[in] dim   - dimension to extract

    \return function_class of the approximation
***************************************************************/
enum function_class 
multi_approx_opts_get_fc(const struct MultiApproxOpts * fargs, size_t dim)
{
    return fargs->aopts[dim]->fc;
}

/***********************************************************//**
    Extract the approximation arguments to use for the approximation of the
    *dim*-th dimensional functions 

    \param[in] fargs - function train approximation arguments
    \param[in] dim   - dimension to extract

    \return approximation arguments
***************************************************************/
void * multi_approx_opts_get_aopts(const struct MultiApproxOpts * fargs, 
                                   size_t dim)
{
    assert (fargs != NULL);
    return fargs->aopts[dim];
}

/***********************************************************//**
    Get the dimension
***************************************************************/
size_t multi_approx_opts_get_dim(const struct MultiApproxOpts * f)
{
    assert (f != NULL);
    return f->dim;
}
